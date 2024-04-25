// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlSignature.h"
#include "GameFramework/Actor.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"

int32 FRCSignature::AddFieldsFromEntities(URemoteControlPreset* InPreset, TConstArrayView<FGuid> InFieldEntityIds)
{
	if (!InPreset || InFieldEntityIds.IsEmpty())
	{
		return 0;
	}

	const int32 PreviousFieldCount = Fields.Num();

	Fields.Reserve(PreviousFieldCount + InFieldEntityIds.Num());

	for (const FGuid& EntityId : InFieldEntityIds)
	{
		TSharedPtr<FRemoteControlProperty> ExposedProperty = InPreset->GetExposedEntity<FRemoteControlProperty>(EntityId).Pin();
		if (!ExposedProperty.IsValid())
		{
			continue;
		}

		FRCSignatureField Field;
		Field.bEnabled = true;
		Field.FieldPath = ExposedProperty->FieldPathInfo;
		Field.SupportedClass = ExposedProperty->GetSupportedBindingClass();

		if (UObject* BoundObject = ExposedProperty->GetBoundObject())
		{
			if (AActor* ActorOwner = BoundObject->GetTypedOuter<AActor>())
			{
				Field.ObjectRelativePath = BoundObject->GetPathName(ActorOwner);
			}
		}

		Fields.AddUnique(MoveTemp(Field));
	}

	return Fields.Num() - PreviousFieldCount;
}

int32 FRCSignature::ApplySignature(URemoteControlPreset* InPreset, TConstArrayView<TWeakObjectPtr<AActor>> InActors) const
{
	if (!InPreset || InActors.IsEmpty())
	{
		return 0;
	}

	// Resolve Weak Actors
	TArray<AActor*> ResolvedActors;
	ResolvedActors.Reserve(InActors.Num());
	for (const TWeakObjectPtr<AActor>& ActorWeak : InActors)
	{
		if (AActor* Actor = ActorWeak.Get())
		{
			ResolvedActors.Add(Actor);
		}
	}

	if (ResolvedActors.IsEmpty())
	{
		return false;
	}

	int32 ExposeCount = 0;

	FRemoteControlPresetExposeArgs ExposeArgs;

	for (const FRCSignatureField& Field : Fields)
	{
		if (!Field.bEnabled)
		{
			continue;
		}

		// Resolve, not load. It should be loaded already if relevant to the Actors to apply this signature to
		UClass* SupportedClass = Field.SupportedClass.ResolveClass();
		UClass* FindClass = SupportedClass ? SupportedClass : UObject::StaticClass();

		FRCFieldPathInfo Path = Field.FieldPath;
		for (AActor* Actor : ResolvedActors)
		{
			UObject* Context;
			if (Field.ObjectRelativePath.IsEmpty())
			{
				Context = Actor;
			}
			else
			{
				Context = StaticFindObject(FindClass, Actor, *Field.ObjectRelativePath);

				// Slow Path: if Subobject Path did not find the object, try to find the first sub-object of the actor matching the class.
				if (!Context && SupportedClass)
				{
					ForEachObjectWithOuterBreakable(Actor, [&Context, SupportedClass](UObject* InSubobject)->bool
						{
							UClass* SubobjectClass = InSubobject->GetClass();
							if (SubobjectClass && SubobjectClass->IsChildOf(SupportedClass))
							{
								Context = InSubobject;
								return false;
							}
							return true;
						}
						, /*bIncludeNestedObjects*/true);
				}
			}

			if (Context && Path.Resolve(Context))
			{
				InPreset->ExposeProperty(Context, Field.FieldPath, ExposeArgs);
				++ExposeCount;
			}
		}
	}

	return ExposeCount;
}
