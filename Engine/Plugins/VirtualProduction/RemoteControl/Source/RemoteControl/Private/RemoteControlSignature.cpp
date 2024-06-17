// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlSignature.h"
#include "GameFramework/Actor.h"
#include "RemoteControlField.h"
#include "RemoteControlPreset.h"

FRCSignatureField FRCSignatureField::CreateField(const FRCFieldPathInfo& InFieldPathInfo, UObject* InOwnerObject, FProperty* InProperty)
{
	UClass* SupportedBindingClass = nullptr;

	if (InProperty)
	{
		if (UClass* PropertyOwnerClass = InProperty->GetOwnerClass())
		{
			SupportedBindingClass = PropertyOwnerClass;
		}
	}

	return CreateField(InFieldPathInfo, InOwnerObject, SupportedBindingClass);
}

FRCSignatureField FRCSignatureField::CreateField(const FRCFieldPathInfo& InFieldPathInfo, UObject* InOwnerObject, UClass* InSupportedClass)
{
	if (!InSupportedClass && InFieldPathInfo.GetSegmentCount() > 0 && InFieldPathInfo.GetFieldSegment(0).IsResolved())
	{
		InSupportedClass = InFieldPathInfo.GetFieldSegment(0).ResolvedData.Field->GetOwnerClass();
	}

	FRCSignatureField Field;
	Field.bEnabled = true;
	Field.FieldPath = InFieldPathInfo;
	Field.SupportedClass = InSupportedClass;

	if (UObject* BoundObject = InOwnerObject)
	{
		if (AActor* ActorOwner = BoundObject->GetTypedOuter<AActor>())
		{
			Field.ObjectRelativePath = BoundObject->GetPathName(ActorOwner);
		}
	}

	return Field;
}

int32 FRCSignature::AddFields(TConstArrayView<FRCSignatureField> InFields)
{
	const int32 PreviousNum = Fields.Num();
	Fields.Reserve(PreviousNum + InFields.Num());

	for (const FRCSignatureField& Field : InFields)
	{
		Fields.AddUnique(Field);
	}

	return Fields.Num() - PreviousNum;
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
