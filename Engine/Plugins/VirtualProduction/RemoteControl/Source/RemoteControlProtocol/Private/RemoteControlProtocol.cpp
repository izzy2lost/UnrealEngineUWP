// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlProtocol.h"

#include "GameFramework/Actor.h"
#include "Misc/CoreDelegates.h"
#include "RemoteControlPreset.h"
#include "RemoteControlProtocolBinding.h"
#include "RemoteControlProtocolModule.h"
#include "RemoteControlSettings.h"
#include "UObject/StructOnScope.h"

namespace UE::RemoteControl::Protocol::Private
{
	/** Struct that updates actors on scope, useful when applying protocol values in performance mode */
	struct FRCScopedUpdateActors
	{
		FRCScopedUpdateActors(const TMap<const FRemoteControlProtocolEntityPtr, double>& EntityToValueMap)
		{
			for (const TPair<const FRemoteControlProtocolEntityPtr, double>& EntityValuesToApplyPair : EntityToValueMap)
			{
				if (EntityValuesToApplyPair.Key.IsValid() && EntityValuesToApplyPair.Key->IsValid())
				{
					FRemoteControlProtocolEntity* ProtocolEntity = EntityValuesToApplyPair.Key->Get();
					URemoteControlPreset* Preset = ProtocolEntity->GetOwner().Get();
					if (!Preset)
					{
						continue;
					}

					const TSharedPtr<FRemoteControlProperty> RemoteControlProperty = Preset->GetExposedEntity<FRemoteControlProperty>(ProtocolEntity->GetPropertyId()).Pin();
					if (!RemoteControlProperty.IsValid())
					{
						continue;
					}

					for (UObject* Object : RemoteControlProperty->GetBoundObjects())
					{
						if (AActor* Actor = Object->GetTypedOuter<AActor>())
						{
							Actors.AddUnique(Actor);
						}
					}
				}
			}
		}

		~FRCScopedUpdateActors()
		{
			for (AActor* Actor : Actors)
			{
				Actor->UpdateComponentTransforms();
				Actor->MarkComponentsRenderStateDirty();
			}
		}

	private:
		/** The actors which need update */
		TArray<AActor*> Actors;
	};
}

FRemoteControlProtocol::FRemoteControlProtocol(const FName InProtocolName)
	: ProtocolName(InProtocolName)
{
	FCoreDelegates::OnEndFrame.AddRaw(this, &FRemoteControlProtocol::OnEndFrame);
}

FRemoteControlProtocol::~FRemoteControlProtocol()
{
	FCoreDelegates::OnEndFrame.RemoveAll(this);
}

void FRemoteControlProtocol::Init()
{
#if WITH_EDITOR

	RegisterColumns();

#endif // WITH_EDITOR
}

FRemoteControlProtocolEntityPtr FRemoteControlProtocol::CreateNewProtocolEntity(FProperty* InProperty, URemoteControlPreset* InOwner, FGuid InPropertyId) const
{
	FRemoteControlProtocolEntityPtr NewDataPtr = MakeShared<TStructOnScope<FRemoteControlProtocolEntity>>();
	NewDataPtr->InitializeFrom(FStructOnScope(GetProtocolScriptStruct()));
	(*NewDataPtr)->Init(InOwner, InPropertyId);

	return NewDataPtr;
}

void FRemoteControlProtocol::QueueValue(const FRemoteControlProtocolEntityPtr InProtocolEntity, const double InProtocolValue)
{
	EntityValuesToApply.Add(InProtocolEntity, InProtocolValue);
}

void FRemoteControlProtocol::OnEndFrame()
{
	using namespace UE::RemoteControl::Protocol::Private;
	
	const bool bPerformancMode = GetDefault<URemoteControlSettings>()->bProtocolPerformanceMode;
	TOptional<FRCScopedUpdateActors> OptionalScopedUpdateActors;
	if (bPerformancMode)
	{
		OptionalScopedUpdateActors = FRCScopedUpdateActors(EntityValuesToApply);
	}

	const ERCModifyOperationFlags ModifyOperationFlags = bPerformancMode ? ERCModifyOperationFlags::SkipPropertyChangeEvents : ERCModifyOperationFlags::None;
	for (const TPair<const FRemoteControlProtocolEntityPtr, double>& EntityValuesToApplyPair : EntityValuesToApply)
	{
		// Check is the Shared ptr and TStructOnScope is valid
		if (EntityValuesToApplyPair.Key.IsValid() && EntityValuesToApplyPair.Key->IsValid())
		{
			FRemoteControlProtocolEntity* ProtocolEntity = EntityValuesToApplyPair.Key->Get();
			const double ThisFrameValue = EntityValuesToApplyPair.Value;
			const double* PreviousFrameValuePtr = PreviousTickValuesToApply.Find(EntityValuesToApplyPair.Key);
			
			// Check the value from previous frame
			if (PreviousFrameValuePtr == nullptr || !FMath::IsNearlyEqual(ThisFrameValue, *PreviousFrameValuePtr))
			{
				if (!ProtocolEntity->ApplyProtocolValueToProperty(EntityValuesToApplyPair.Value, ModifyOperationFlags))
				{
					// Warn if the the value can't by applied
					ensureMsgf(false, TEXT("Can't apply property for Protocol %s and PropertyId %s"),
					           *ProtocolName.ToString(), *ProtocolEntity->GetPropertyId().ToString());
				}
			}
		}
	}

	// Move the values from this frame to cached map
	PreviousTickValuesToApply = MoveTemp(EntityValuesToApply);
}

#if WITH_EDITOR

FProtocolColumnPtr FRemoteControlProtocol::GetRegisteredColumn(const FName& ByColumnName) const
{
	for (const FProtocolColumnPtr& ProtocolColumn : RegisteredColumns)
	{
		if (ProtocolColumn->ColumnName == ByColumnName)
		{
			return ProtocolColumn.ToSharedRef();
		}
	}

	return nullptr;
}

void FRemoteControlProtocol::GetRegisteredColumns(TSet<FName>& OutColumns)
{
	for (const FProtocolColumnPtr& ProtocolColumn : RegisteredColumns)
	{
		OutColumns.Add(ProtocolColumn->ColumnName);
	}
}

#endif // WITH_EDITOR

TFunction<bool(FRemoteControlProtocolEntityWeakPtr InProtocolEntityWeakPtr)> FRemoteControlProtocol::CreateProtocolComparator(FGuid InPropertyId)
{
	return [InPropertyId](FRemoteControlProtocolEntityWeakPtr InProtocolEntityWeakPtr) -> bool
	{
		if (FRemoteControlProtocolEntityPtr ProtocolEntityPtr = InProtocolEntityWeakPtr.Pin())
		{
			return InPropertyId == (*ProtocolEntityPtr)->GetPropertyId();
		}

		return false;
	};
}

FProperty* IRemoteControlProtocol::GetRangeInputTemplateProperty() const
{
	FProperty* RangeInputTemplateProperty = nullptr;
	if(UScriptStruct* ProtocolScriptStruct = GetProtocolScriptStruct())
	{
		RangeInputTemplateProperty = ProtocolScriptStruct->FindPropertyByName("RangeInputTemplate");
	}

	if(!ensure(RangeInputTemplateProperty))
	{
		UE_LOG(LogRemoteControlProtocol, Warning, TEXT("Could not find RangeInputTemplate Property for this Protocol. Please either add this named property to the ProtocolScriptStruct implementation, or override IRemoteControlProtocol::GetRangeTemplateType."));
	}
	
	return RangeInputTemplateProperty;
}
