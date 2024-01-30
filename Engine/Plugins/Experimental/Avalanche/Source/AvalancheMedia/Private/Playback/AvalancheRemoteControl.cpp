// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvalancheRemoteControl.h"

#include "Action/RCAction.h"
#include "Action/RCActionContainer.h"
#include "Action/RCPropertyIdAction.h"
#include "AvaSerializationUtils.h"
#include "Backends/JsonStructDeserializerBackend.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "Behaviour/RCBehaviour.h"
#include "Controller/RCController.h"
#include "IRemoteControlModule.h"
#include "RCVirtualProperty.h"
#include "RemoteControlPropertyIdRegistry.h"

DEFINE_LOG_CATEGORY(LogAvaMediaRemoteControl);

namespace UE::AvalancheRemoteControl::Private
{
	bool ResolveObjectPropertyForReadOnly(UObject* Object, FRCFieldPathInfo PropertyPath, FRCObjectReference& OutObjectRef, FString& OutErrorText)
	{
		if (!Object)
		{
			OutErrorText = FString::Printf(TEXT("Invalid object to resolve property '%s'"), *PropertyPath.GetFieldName().ToString());
			return false;
		}

		bool bSuccess = true;
		
		if (PropertyPath.GetSegmentCount() != 0)
		{
			if (PropertyPath.Resolve(Object))
			{
				OutObjectRef = FRCObjectReference{ERCAccess::READ_ACCESS, Object, MoveTemp(PropertyPath)};
			}
			else
			{
				OutErrorText = FString::Printf(TEXT("Object property: %s could not be resolved on object: %s"), *PropertyPath.GetFieldName().ToString(), *Object->GetPathName());
				bSuccess = false;
			}
		}
		else
		{
			OutObjectRef = FRCObjectReference{ERCAccess::READ_ACCESS, Object};
		}
		return bSuccess;
	}
	
	inline bool GetObjectRef(const TSharedPtr<const FRemoteControlProperty>& InField, const ERCAccess InAccess, FRCObjectReference& OutObjectRef)
	{
		if (!InField.IsValid())
		{
			return false;
		}
		if (UObject* FieldBoundObject = InField->GetBoundObject(); IsValid(FieldBoundObject))
		{
			FString ErrorText;
			bool bSuccess = false;
			
			if (InAccess == ERCAccess::READ_ACCESS)
			{
				// Fix for private/protected properties: we allow reading the property even if private/protected.
				// This is necessary for the case of private/protect props that have a setter.
				// RC doesn't have a path to allow reading those (it doesn't support getters).
				bSuccess = ResolveObjectPropertyForReadOnly(FieldBoundObject, InField->FieldPathInfo, OutObjectRef, ErrorText);
			}
			else
			{
				bSuccess = IRemoteControlModule::Get().ResolveObjectProperty(InAccess, FieldBoundObject, InField->FieldPathInfo, OutObjectRef, &ErrorText);
			}

			if (bSuccess)
			{
				return true;
			}
			
			UE_LOG(LogAvaMediaRemoteControl, Error,
				TEXT("Couldn\'t resolve object property \"%s\" in object \"%s\": %s"),
				*InField->FieldName.ToString(), *FieldBoundObject->GetPathName(), *ErrorText);
		}
		else
		{
			UE_LOG(LogAvaMediaRemoteControl, Error,
				TEXT("Couldn\'t resolve object property \"%s\": Invalid Field Bound Object."),
				*InField->FieldName.ToString());
		}
		return false;
	}
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::GetValueOfEntity(const TSharedPtr<const FRemoteControlEntity>& RemoteControlEntity, TArray<uint8>& OutValue)
{
	using namespace UE::AvalancheRemoteControl::Private;
	OutValue.Reset();

	TSharedPtr<const FRemoteControlProperty> Field = StaticCastSharedPtr<const FRemoteControlProperty>(RemoteControlEntity);
	if (!Field.IsValid())
	{
		return EAvaRemoteControlResult::InvalidParameter;
	}

	FRCObjectReference ObjectRef;
	if (!GetObjectRef(Field, ERCAccess::READ_ACCESS, ObjectRef))
	{
		return EAvaRemoteControlResult::ReadAccessDenied;
	}
	FMemoryWriter Writer = FMemoryWriter(OutValue);
	FJsonStructSerializerBackend WriterBackend = FJsonStructSerializerBackend(Writer, EStructSerializerBackendFlags::Default);
	return IRemoteControlModule::Get().GetObjectProperties(ObjectRef, WriterBackend) ? EAvaRemoteControlResult::Completed : EAvaRemoteControlResult::ReadPropertyFailed;
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::GetValueOfEntity(const TSharedPtr<const FRemoteControlEntity>& InRemoteControlEntity, FString& OutValue)
{
	TArray<uint8> ValueAsBytes;
	const EAvaRemoteControlResult Result = GetValueOfEntity(InRemoteControlEntity, ValueAsBytes);
	AvaSerializationUtils::JsonValueConversion::BytesToString(ValueAsBytes, OutValue);
	return Result;
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::SetValueOfEntity(const TSharedPtr<FRemoteControlEntity>& RemoteControlEntity, const TArrayView<const uint8>& InValue)
{
	using namespace UE::AvalancheRemoteControl::Private;

	TSharedPtr<FRemoteControlProperty> Field = StaticCastSharedPtr<FRemoteControlProperty>(RemoteControlEntity);
	if (!Field.IsValid())
	{
		return EAvaRemoteControlResult::InvalidParameter;
	}
	
	FRCObjectReference ObjectRefRead;
	if (GetObjectRef(Field, ERCAccess::READ_ACCESS, ObjectRefRead))
	{
		TArray<uint8> CurrentValueAsBytes;
		FMemoryWriter Writer(CurrentValueAsBytes);
		FJsonStructSerializerBackend WriterBackend = FJsonStructSerializerBackend(Writer, EStructSerializerBackendFlags::Default);
		if (IRemoteControlModule::Get().GetObjectProperties(ObjectRefRead, WriterBackend))
		{
			if (CurrentValueAsBytes == InValue)
			{
				// if the given value is already set, don't do anything
				return EAvaRemoteControlResult::UpToDate;
			}
		}
	}

	FRCObjectReference ObjectRefWrite;
	if (!GetObjectRef(Field, ERCAccess::WRITE_ACCESS, ObjectRefWrite))
	{
		return EAvaRemoteControlResult::WriteAccessDenied;
	}

	FMemoryReaderView Reader(InValue);
	FJsonStructDeserializerBackend ReaderBackend = FJsonStructDeserializerBackend(Reader);

	// Notes:
	// - if RemoteControl.EnableOngoingChangeOptimization is enabled, PostEditChangeProperty is not called right away
	// there might be a delay (of 0.2 seconds) before it is called.
	// - OnPropertyChangedDelegate (OnExposedPropertiesModified()) is a "per frame" event and is broadcast from URemoteControlPreset::OnEndFrame().
	const bool bDeserializationSucceeded = IRemoteControlModule::Get().SetObjectProperties(ObjectRefWrite, ReaderBackend, ERCPayloadType::Json);

#if WITH_EDITOR
	UObject* const Object = ObjectRefWrite.Object.Get();
	if (bDeserializationSucceeded && IsValid(Object))
	{
		FEditPropertyChain EditPropertyChain;
		ObjectRefWrite.PropertyPathInfo.ToEditPropertyChain(EditPropertyChain);

		// Note: Only PostEditChangeChainProperty is called here because PostEditChangeProperty is already dealt with in RC.
		// Ideally, this should be in RC so that PostEditChangeProperty does not get called twice.
		if (!EditPropertyChain.IsEmpty())
		{
			FPropertyChangedEvent PropertyEvent = ObjectRefWrite.PropertyPathInfo.ToPropertyChangedEvent(EPropertyChangeType::ValueSet);

			FPropertyChangedChainEvent ChainEvent(EditPropertyChain, PropertyEvent);

			TArray<TMap<FString, int32>> ArrayIndicesPerObject;
			{
				TMap<FString, int32> ArrayIndices;
				ArrayIndices.Reserve(ObjectRefWrite.PropertyPathInfo.Segments.Num());

				for (FRCFieldPathSegment& Segment : ObjectRefWrite.PropertyPathInfo.Segments)
				{
					ArrayIndices.Add(Segment.Name.ToString(), Segment.ArrayIndex);
				}

				ArrayIndicesPerObject.Add(MoveTemp(ArrayIndices));
			}

			ChainEvent.ObjectIteratorIndex = 0;
			ChainEvent.SetArrayIndexPerObject(ArrayIndicesPerObject);

			Object->PostEditChangeChainProperty(ChainEvent);
		}
	}
#endif

	return bDeserializationSucceeded ? EAvaRemoteControlResult::Completed : EAvaRemoteControlResult::WritePropertyFailed;
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::SetValueOfEntity(const TSharedPtr<FRemoteControlEntity>& InRemoteControlEntity, const FString& InValue)
{
	return SetValueOfEntity(InRemoteControlEntity, AvaSerializationUtils::JsonValueConversion::ValueToConstBytesView(InValue));
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::GetValueOfController(URCVirtualPropertyBase* InController, TArray<uint8>& OutValue)
{
	OutValue.Reset();

	if (!InController)
	{
		return EAvaRemoteControlResult::InvalidParameter;
	}

	FMemoryWriter Writer = FMemoryWriter(OutValue);
	FJsonStructSerializerBackend WriterBackend = FJsonStructSerializerBackend(Writer, EStructSerializerBackendFlags::Default);
	InController->SerializeToBackend(WriterBackend);
	return EAvaRemoteControlResult::Completed;
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::GetValueOfController(URCVirtualPropertyBase* InController, FString& OutValue)
{
	TArray<uint8> ValueAsBytes;
	const EAvaRemoteControlResult Result = GetValueOfController(InController, ValueAsBytes);

	AvaSerializationUtils::JsonValueConversion::BytesToString(ValueAsBytes, OutValue);
	return Result;
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::SetValueOfController(URCVirtualPropertyBase* InController, const TArrayView<const uint8>& InValue)
{
	if (!InController)
	{
		return EAvaRemoteControlResult::InvalidParameter;
	}

	// TODO: Use the RC error callback to catch errors in the controller actions.
	FMemoryReaderView Reader(InValue);
	FJsonStructDeserializerBackend ReaderBackend = FJsonStructDeserializerBackend(Reader);
	return InController->DeserializeFromBackend(ReaderBackend) ? EAvaRemoteControlResult::Completed : EAvaRemoteControlResult::WritePropertyFailed;
}

EAvaRemoteControlResult UE::AvalancheRemoteControl::SetValueOfController(URCVirtualPropertyBase* InController, const FString& InValue)
{
	return SetValueOfController(InController, AvaSerializationUtils::JsonValueConversion::ValueToConstBytesView(InValue));
}

bool UE::AvalancheRemoteControl::GetEntitiesControlledByController(const URemoteControlPreset* InRemoteControlPreset, const URCVirtualPropertyBase* InVirtualProperty, TSet<FGuid>& OutEntityIds)
{
	const URCController* Controller = Cast<URCController>(InVirtualProperty);
	if (!IsValid(Controller))
	{
		return false;
	}

	for (const TObjectPtr<URCBehaviour>& Behaviour : Controller->Behaviours)
	{
		const TSet<TObjectPtr<URCAction>>& Actions = Behaviour->ActionContainer->GetActions();
		for (const TObjectPtr<URCAction>& Action : Actions)
		{
			if (Action)
			{
				if (const URCPropertyIdAction* IdentityAction = Cast<URCPropertyIdAction>(Action.Get()))
				{
					if (const URemoteControlPropertyIdRegistry* IdentityRegistry = InRemoteControlPreset->GetPropertyIdRegistry())
					{
						OutEntityIds.Append(IdentityRegistry->GetEntityIdsForPropertyId(IdentityAction->PropertyId));
					}
				}
				else
				{
					OutEntityIds.Add(Action->ExposedFieldId);
				}
			}
		}
	}
	return true;
}

FString UE::AvalancheRemoteControl::EnumToString(EAvaRemoteControlResult InValue)
{
	return StaticEnum<EAvaRemoteControlResult>()->GetNameStringByValue(static_cast<int64>(InValue));
}

namespace UE::AvalancheRemoteControl
{
	FScopedPushControllerBehavioursEnable::FScopedPushControllerBehavioursEnable(URCVirtualPropertyBase* InVirtualProperty, bool bInBehavioursEnabled)
		: VirtualProperty(InVirtualProperty)
	{
		if (URCController* const Controller = Cast<URCController>(InVirtualProperty))
		{
			PreviousBehavioursEnabled.Reserve(Controller->Behaviours.Num());			
			for (URCBehaviour* const Behavior : Controller->Behaviours)
			{
				PreviousBehavioursEnabled.Add(Behavior->bIsEnabled);
				Behavior->bIsEnabled = bInBehavioursEnabled;
			}
		}
	}

	FScopedPushControllerBehavioursEnable::~FScopedPushControllerBehavioursEnable()
	{
		if (URCController* const Controller = Cast<URCController>(VirtualProperty))
		{
			int32 BehaviourIndex = 0;
			for (URCBehaviour* const Behavior : Controller->Behaviours)
			{
				Behavior->bIsEnabled = PreviousBehavioursEnabled[BehaviourIndex++];
			}
		}
	}
}