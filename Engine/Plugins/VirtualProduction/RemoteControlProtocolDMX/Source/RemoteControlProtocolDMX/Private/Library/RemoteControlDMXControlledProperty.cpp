// Copyright Epic Games, Inc. All Rights Reserved.

#include "Library/RemoteControlDMXControlledProperty.h"

#include "GameFramework/Actor.h"
#include "RemoteControlField.h"
#include "RemoteControlProtocolDMX.h"

namespace UE::RemoteControl::DMX
{
	FRemoteControlDMXControlledProperty::FRemoteControlDMXControlledProperty(const TSharedRef<const FRemoteControlProperty> InExposedProperty)
		: ExposedProperty(InExposedProperty)
	{
		Algo::TransformIf(ExposedProperty->ProtocolBindings, Entities,
			[](const FRemoteControlProtocolBinding& Binding)
			{
				return
					Binding.GetProtocolName() == FRemoteControlProtocolDMX::ProtocolName &&
					Binding.GetRemoteControlProtocolEntityPtr().IsValid() && 
					Binding.GetRemoteControlProtocolEntityPtr()->IsValid();
			},
			[](const FRemoteControlProtocolBinding& Binding)
			{
				return Binding.GetRemoteControlProtocolEntityPtr().ToSharedRef();
			});

		InitializeNewEntities();
		UnifyEntities();
	}

	const UObject* FRemoteControlDMXControlledProperty::GetOwnerObject() const
	{
		const UObject* BoundObject = ExposedProperty->GetBoundObject();
		if (!BoundObject)
		{
			return nullptr;
		}

		if (const AActor* OwnerActor = BoundObject->GetTypedOuter<AActor>())
		{
			return OwnerActor;
		}
		else if (const AActor* Actor = Cast<AActor>(BoundObject))
		{
			return Actor;
		}
		else
		{
			return BoundObject;
		}
	}

	UDMXEntityFixturePatch* FRemoteControlDMXControlledProperty::GetFixturePatch() const
	{
		const FRemoteControlDMXProtocolEntity* FirstDMXEntity = !Entities.IsEmpty() && Entities[0]->IsValid() ? Entities[0]->Cast<FRemoteControlDMXProtocolEntity>() : nullptr;
		return FirstDMXEntity ? FirstDMXEntity->ExtraSetting.FixturePatchReference.GetFixturePatch() : nullptr;
	}

	int32 FRemoteControlDMXControlledProperty::GetEntityIndexChecked(const TSharedRef<TStructOnScope<FRemoteControlProtocolEntity>>& Entity)
	{
		const int32 EntityIndex = Entities.IndexOfByKey(Entity);
		check(EntityIndex != INDEX_NONE);

		return EntityIndex;
	}

	void FRemoteControlDMXControlledProperty::InitializeNewEntities()
	{
		for (int32 EntityIndex = 0; EntityIndex < Entities.Num(); EntityIndex++)
		{
			FRemoteControlDMXProtocolEntity* DMXEntity = Entities[EntityIndex]->IsValid() ? Entities[EntityIndex]->Cast<FRemoteControlDMXProtocolEntity>() : nullptr;
			if (!DMXEntity)
			{
				continue;
			}

			if (DMXEntity->ExtraSetting.FunctionIndex == INDEX_NONE)
			{
				const FString AttributeName = [EntityIndex, this]()
					{
						if (Entities.Num() == 1)
						{
							return FString::Printf(TEXT("%s.%s"), *GetSubobjectPath(), *ExposedProperty->FieldPathInfo.ToString());
						}
						else
						{
							return FString::Printf(TEXT("%s.%s%i"), *GetSubobjectPath(), *ExposedProperty->FieldPathInfo.ToString(), EntityIndex + 1);
						}
					}();

				DMXEntity->ExtraSetting.AttributeName = *AttributeName;
				DMXEntity->ExtraSetting.FunctionIndex = EntityIndex;
			}
		}
	}

	void FRemoteControlDMXControlledProperty::UnifyEntities()
	{
		const FRemoteControlDMXProtocolEntity* FirstDMXEntity = !Entities.IsEmpty() && Entities[0]->IsValid() ? Entities[0]->Cast<FRemoteControlDMXProtocolEntity>() : nullptr;
		if (!FirstDMXEntity)
		{
			return;
		}

		UDMXEntityFixturePatch* UnifiedFixturePatch = FirstDMXEntity->ExtraSetting.FixturePatchReference.GetFixturePatch();
		const int32 UnifiedFunctionIndex = FirstDMXEntity->ExtraSetting.FunctionIndex;
		const bool bUnifiedIsPrimaryPatch = FirstDMXEntity->ExtraSetting.bIsPrimaryPatch;

		for (const TSharedRef<TStructOnScope<FRemoteControlProtocolEntity>>& Entity : Entities)
		{
			FRemoteControlDMXProtocolEntity* DMXEntity = Entity->IsValid() ? Entity->Cast<FRemoteControlDMXProtocolEntity>() : nullptr;
			if (!DMXEntity || DMXEntity == FirstDMXEntity)
			{
				continue;
			}

			// Only update if the properties differ, as this will trigger another rebuild of the library proxy that uses this object
			if (DMXEntity->ExtraSetting.FixturePatchReference.GetFixturePatch() != UnifiedFixturePatch ||
				DMXEntity->ExtraSetting.FunctionIndex != UnifiedFunctionIndex ||
				DMXEntity->ExtraSetting.bIsPrimaryPatch != bUnifiedIsPrimaryPatch)
			{
				ensureMsgf(0, TEXT("Found entities of the same property with differing properties. Adopting property from first entity to mend."));

				DMXEntity->ExtraSetting.FixturePatchReference = UnifiedFixturePatch;
				DMXEntity->ExtraSetting.FunctionIndex = UnifiedFunctionIndex;
				DMXEntity->ExtraSetting.bIsPrimaryPatch = bUnifiedIsPrimaryPatch;
			}
		}
	}

	FString FRemoteControlDMXControlledProperty::GetSubobjectPath() const
	{
		if (const UObject* OwnerObject = GetOwnerObject())
		{
			const FString OwnerName = OwnerObject->GetFName().ToString();
			const FString BindingPath = ExposedProperty->GetLastBindingPath().ToString();

			const int32 OwnerNameIndex = BindingPath.Find(OwnerName + TEXT("."));
			return BindingPath.RightChop(OwnerNameIndex + OwnerName.Len() + 1);
		}

		return TEXT("");
	}
}
