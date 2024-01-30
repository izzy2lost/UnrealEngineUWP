// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvalancheRemoteControlValues.h"

#include "AvaSerializationUtils.h"
#include "Playback/AvalancheRemoteControl.h"
#include "Playback/AvalancheRemoteControlValuesPrivate.h"
#include "RCVirtualProperty.h"
#include "Serialization/CustomVersion.h"

namespace UE::AvalancheRemoteControlValues::Private
{
	bool PruneValues(const TMap<FGuid, FAvalancheRemoteControlValue>& InValues, TMap<FGuid, FAvalancheRemoteControlValue>& OutValues)
	{
		bool bModified = false;
		for (TMap<FGuid, FAvalancheRemoteControlValue>::TIterator ValueIterator(OutValues); ValueIterator; ++ValueIterator)
		{
			if (!InValues.Contains(ValueIterator->Key))
			{
				ValueIterator.RemoveCurrent();
				bModified = true;
			}
		}
		return bModified;
	}
	
	bool UpdateValues(const TMap<FGuid, FAvalancheRemoteControlValue>& InValues, TMap<FGuid, FAvalancheRemoteControlValue>& OutValues, bool bInUpdateDefaults)
	{
		// Remove property values that are no longer exposed.
		bool bModified = PruneValues(InValues, OutValues);
	
		// Add missing property values and optionally update the default values.
		for (const TPair<FGuid, FAvalancheRemoteControlValue>& SourceValue : InValues)
		{
			FAvalancheRemoteControlValue* ExistingValue = OutValues.Find(SourceValue.Key);
			if (!ExistingValue)
			{
				// Remark: IsDefault flag follows along.
				OutValues.Add(SourceValue.Key, SourceValue.Value);
				bModified = true;
			}
			else if (bInUpdateDefaults && ExistingValue->bIsDefault && !ExistingValue->IsSameValueAs(SourceValue.Value))
			{
				ExistingValue->SetValueFrom(SourceValue.Value);
				bModified = true;
			}
		}
		return bModified;
	}

	bool HasSameValues(const TMap<FGuid, FAvalancheRemoteControlValue>& InValues, const TMap<FGuid, FAvalancheRemoteControlValue>& InOtherValues)
	{
		// If values count differ, consider as different
		if (InValues.Num() != InOtherValues.Num())
		{
			return false;
		}

		// Both Value maps have the same count, so one cannot be a subset of another, therefore, a single find pass should determine equality 
		for (const TPair<FGuid, FAvalancheRemoteControlValue>& Pair : InValues)
		{
			const FAvalancheRemoteControlValue* FoundValue = InOtherValues.Find(Pair.Key);
			if (!FoundValue || !FoundValue->IsSameValueAs(Pair.Value))
			{
				// Other's Value wasn't found, or was different from the value of this
				return false;
			}
		}

		return true;
	}

	EAvaRemoteControlChanges ToRemoteControlChanges(bool bInModified, EAvaRemoteControlChanges InModifiedChanges)
	{
		return bInModified ? InModifiedChanges : EAvaRemoteControlChanges::None;
	}
}

const FGuid FAvalancheRemoteControlValueCustomVersion::Key(0x85218F83, 0xEDF141CA, 0x800EF947, 0x2F14CB06);
FCustomVersionRegistration GRegisterAvalancheRemoteControlValueCustomVersion(FAvalancheRemoteControlValueCustomVersion::Key
	, FAvalancheRemoteControlValueCustomVersion::LatestVersion
	, TEXT("AvalancheRemoteControlValueVersion"));

bool FAvalancheRemoteControlValue::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAvalancheRemoteControlValueCustomVersion::Key);
	
	if (Ar.CustomVer(FAvalancheRemoteControlValueCustomVersion::Key) >= FAvalancheRemoteControlValueCustomVersion::ValueAsString)
	{
		UScriptStruct* Struct = FAvalancheRemoteControlValue::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, reinterpret_cast<uint8*>(this), Struct, nullptr);
	}
	else
	{
		FAvalancheRemoteControlValueAsBytes_Legacy LegacyValue;
		UScriptStruct* Struct = FAvalancheRemoteControlValueAsBytes_Legacy::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, reinterpret_cast<uint8*>(&LegacyValue), Struct, nullptr);

		UE::AvaSerializationUtils::JsonValueConversion::BytesToString(LegacyValue.Bytes, Value);
		bIsDefault = LegacyValue.bIsDefault;
	}

	return true;
}

void FAvalancheRemoteControlValues::RefreshControlledEntities(const URemoteControlPreset* InRemoteControlPreset)
{
	EntitiesControlledByController.Reset();

	if (IsValid(InRemoteControlPreset))
	{
		const TArray<URCVirtualPropertyBase*> Controllers = InRemoteControlPreset->GetControllers();
		for (const URCVirtualPropertyBase* PropertyBase : Controllers)
		{
			if (!UE::AvalancheRemoteControl::GetEntitiesControlledByController(InRemoteControlPreset, PropertyBase, EntitiesControlledByController))
			{
				UE_LOG(LogAvaMediaRemoteControl, Warning, TEXT("Failed to get controlled entities for controller \"%s\" (id:%s)."),
					*PropertyBase->DisplayName.ToString(), *PropertyBase->Id.ToString());
			}
		}
	}
}

void FAvalancheRemoteControlValues::CopyFrom(const URemoteControlPreset* InRemoteControlPreset, bool bInIsDefault)
{
	using namespace UE::AvalancheRemoteControl;

	EntityValues.Reset();
	ControllerValues.Reset();

	RefreshControlledEntities(InRemoteControlPreset);	// will reset ids if invalid.
	
	if (IsValid(InRemoteControlPreset))
	{
		FString ValueAsString;
		for (const TWeakPtr<const FRemoteControlEntity>& EntityWeakPtr : InRemoteControlPreset->GetExposedEntities<FRemoteControlEntity>())
		{
			const TSharedPtr<const FRemoteControlEntity> Entity = EntityWeakPtr.Pin();
			if (!Entity.IsValid())
			{
				continue;
			}

			const EAvaRemoteControlResult Result = GetValueOfEntity(Entity, ValueAsString);

			if (Failed(Result))
			{
				UE_LOG(LogAvaMediaRemoteControl, Error,
					TEXT("Failed to read value of entity \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
					*Entity->GetLabel().ToString(), *Entity->GetId().ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
				continue;
			}
			
			EntityValues.Add(Entity->GetId(), FAvalancheRemoteControlValue(ValueAsString, bInIsDefault));
		}

		TArray<URCVirtualPropertyBase*> Controllers = InRemoteControlPreset->GetControllers();
		for (URCVirtualPropertyBase* Controller : Controllers)
		{
			const EAvaRemoteControlResult Result = GetValueOfController(Controller, ValueAsString);
	
			if (Failed(Result))
			{
				UE_LOG(LogAvaMediaRemoteControl, Error,
					TEXT("Failed to read value of controller \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
					*Controller->DisplayName.ToString(), *Controller->Id.ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
				continue;
			}
			
			ControllerValues.Add(Controller->Id, FAvalancheRemoteControlValue(ValueAsString, bInIsDefault));
		}
	}
}

bool FAvalancheRemoteControlValues::HasSameEntityValues(const FAvalancheRemoteControlValues& InOther) const
{
	return UE::AvalancheRemoteControlValues::Private::HasSameValues(EntityValues, InOther.EntityValues);
}

bool FAvalancheRemoteControlValues::HasSameControllerValues(const FAvalancheRemoteControlValues& InOther) const
{
	return UE::AvalancheRemoteControlValues::Private::HasSameValues(ControllerValues, InOther.ControllerValues);
}

EAvaRemoteControlChanges FAvalancheRemoteControlValues::PruneRemoteControlValues(const FAvalancheRemoteControlValues& InRemoteControlValues)
{
	using namespace UE::AvalancheRemoteControlValues::Private;
	return ToRemoteControlChanges(PruneValues(InRemoteControlValues.EntityValues, EntityValues), EAvaRemoteControlChanges::EntityValues) 
		| ToRemoteControlChanges(PruneValues(InRemoteControlValues.ControllerValues, ControllerValues), EAvaRemoteControlChanges::ControllerValues); 
}

EAvaRemoteControlChanges FAvalancheRemoteControlValues::UpdateRemoteControlValues(const FAvalancheRemoteControlValues& InRemoteControlValues, bool bInUpdateDefaults)
{
	using namespace UE::AvalancheRemoteControlValues::Private;
	return ToRemoteControlChanges(UpdateValues(InRemoteControlValues.EntityValues, EntityValues, bInUpdateDefaults), EAvaRemoteControlChanges::EntityValues) 
		| ToRemoteControlChanges(UpdateValues(InRemoteControlValues.ControllerValues, ControllerValues, bInUpdateDefaults), EAvaRemoteControlChanges::ControllerValues); 
}

bool FAvalancheRemoteControlValues::SetEntityValue(const FGuid& InId, const URemoteControlPreset* InRemoteControlPreset, bool bInIsDefault)
{
	const TSharedPtr<const FRemoteControlEntity> Entity = InRemoteControlPreset->GetExposedEntity<FRemoteControlEntity>(InId).Pin();

	if (!Entity)
	{
		UE_LOG(LogAvaMediaRemoteControl, Error,
			TEXT("Requested entity id \"%s\" was not found in RemoteControlPreset \"%s\"."),
			*InId.ToString(), *InRemoteControlPreset->GetName());
		return false;
	}

	using namespace UE::AvalancheRemoteControl;
	FAvalancheRemoteControlValue Value;
	Value.bIsDefault = bInIsDefault;

	const EAvaRemoteControlResult Result = GetValueOfEntity(Entity, Value.Value);

	if (Failed(Result))
	{
		UE_LOG(LogAvaMediaRemoteControl, Error,
			TEXT("Failed to read value of entity \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
			*Entity->GetLabel().ToString(), *InId.ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
		return false;
	}

	EntityValues.Add(Entity->GetId(), MoveTemp(Value));
	return true;
}

bool FAvalancheRemoteControlValues::SetControllerValue(const FGuid& InId, const URemoteControlPreset* InRemoteControlPreset, bool bInIsDefault)
{
	URCVirtualPropertyBase* Controller = InRemoteControlPreset->GetController(InId);

	if (!Controller)
	{
		UE_LOG(LogAvaMediaRemoteControl, Error,
			TEXT("Requested controller id \"%s\" was not found in RemoteControlPreset \"%s\"."),
			*InId.ToString(), *InRemoteControlPreset->GetName());
		return false;
	}

	using namespace UE::AvalancheRemoteControl;
	FAvalancheRemoteControlValue Value;
	Value.bIsDefault = bInIsDefault;

	const EAvaRemoteControlResult Result = GetValueOfController(Controller, Value.Value);
	
	if (Failed(Result))
	{
		UE_LOG(LogAvaMediaRemoteControl, Error,
			TEXT("Failed to read value of controller \"%s\" (id:%s) from RemoteControlPreset \"%s\": %s."),
			*Controller->DisplayName.ToString(), *InId.ToString(), *InRemoteControlPreset->GetName(), *EnumToString(Result));
		return false;
	}
		
	ControllerValues.Add(InId, MoveTemp(Value));
	return true;
}

void FAvalancheRemoteControlValues::ApplyEntityValuesToRemoteControlPreset(URemoteControlPreset* InRemoteControlPreset) const
{
	if (!InRemoteControlPreset)
	{
		return;
	}
	using namespace UE::AvalancheRemoteControl;
	for (const TWeakPtr<FRemoteControlEntity>& EntityWeakPtr : InRemoteControlPreset->GetExposedEntities<FRemoteControlEntity>())
	{
		if (const TSharedPtr<FRemoteControlEntity> Entity = EntityWeakPtr.Pin())
		{
			if (const FAvalancheRemoteControlValue* Value = GetEntityValue(Entity->GetId()))
			{
				const EAvaRemoteControlResult Result = SetValueOfEntity(Entity, Value->Value); 
				if (Failed(Result))
				{
					UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Failed to set value of exposed entity \"%s\" (id:%s): %s."),
						*Entity->GetLabel().ToString(), *Entity->GetId().ToString(), *EnumToString(Result));
				}
			}
			else
			{
				UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Exposed entity \"%s\" (id:%s): value not found in page."),
					*Entity->GetLabel().ToString(), *Entity->GetId().ToString());
			}
		}
	}
}

void FAvalancheRemoteControlValues::ApplyControllerValuesToRemoteControlPreset(URemoteControlPreset* InRemoteControlPreset, bool bInForceDisableBehaviors) const
{
	if (!InRemoteControlPreset)
	{
		return;
	}
	TArray<URCVirtualPropertyBase*> Controllers = InRemoteControlPreset->GetControllers();
	for (URCVirtualPropertyBase* Controller : Controllers)
	{
		if (const FAvalancheRemoteControlValue* Value = GetControllerValue(Controller->Id))
		{
			using namespace UE::AvalancheRemoteControl;
			EAvaRemoteControlResult Result;
			if (bInForceDisableBehaviors)
			{
				FScopedPushControllerBehavioursEnable PushBehavioursEnable(Controller, false);
				Result = SetValueOfController(Controller, Value->Value);
			}
			else
			{
				Result = SetValueOfController(Controller, Value->Value);
			}
			if (Failed(Result))
			{
				UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Failed to set virtual value of controller \"%s\" (id:%s): %s."),
					*Controller->DisplayName.ToString(), *Controller->Id.ToString(), *EnumToString(Result));
			}
		}
		else
		{
			UE_LOG(LogAvaMediaRemoteControl, Error, TEXT("Controller \"%s\" (id:%s): value not found in page."),
				*Controller->DisplayName.ToString(), *Controller->Id.ToString());
		}
	}
}

bool FAvalancheRemoteControlValues::HasIdCollisions(const FAvalancheRemoteControlValues& InOtherValues) const
{
	const bool bHasControllerIdCollisions = HasIdCollisions(ControllerValues, InOtherValues.ControllerValues);
	const bool bHasEntityIdCollisions = HasIdCollisions(EntityValues, InOtherValues.EntityValues);
	return bHasControllerIdCollisions || bHasEntityIdCollisions; 
}

bool FAvalancheRemoteControlValues::Merge(const FAvalancheRemoteControlValues& InOtherValues)
{
	const bool bHasIdCollisions = HasIdCollisions(InOtherValues);

	ControllerValues.Append(InOtherValues.ControllerValues);
	EntityValues.Append(InOtherValues.EntityValues);		
	EntitiesControlledByController.Append(InOtherValues.EntitiesControlledByController);

	return !bHasIdCollisions;
}

bool FAvalancheRemoteControlValues::HasIdCollisions(const TMap<FGuid, FAvalancheRemoteControlValue>& InValues, const TMap<FGuid, FAvalancheRemoteControlValue>& InOtherValues)
{
	for (const TPair<FGuid, FAvalancheRemoteControlValue>& ValueEntry : InValues)
	{
		if (InOtherValues.Contains(ValueEntry.Key))
		{
			return true;
		}
	}
	return false;
}

const FAvalancheRemoteControlValues& FAvalancheRemoteControlValues::GetDefaultEmpty()
{
	const static FAvalancheRemoteControlValues Empty;
	return Empty;
}
