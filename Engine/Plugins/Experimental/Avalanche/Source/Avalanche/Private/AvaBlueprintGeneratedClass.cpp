// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBlueprintGeneratedClass.h"
#include "AvaActor.h"
#include "AvaSequence.h"

void UAvaBlueprintGeneratedClass::UpdateProperties(AAvaActor* InActor)
{
	UAvaBlueprintGeneratedClass* const GeneratedClass = Cast<UAvaBlueprintGeneratedClass>(InActor->GetClass());

	TMap<FName, FObjectPropertyBase*> ObjectPropertyMap;
	for (TFieldIterator<FObjectPropertyBase> Iter(GeneratedClass, EFieldIterationFlags::Default); Iter; ++Iter)
	{
		check(*Iter);
		ensureMsgf(!ObjectPropertyMap.Contains(Iter->GetFName()), TEXT("There are properties with the same names: '%s'"), *Iter->GetName());
		ObjectPropertyMap.Add(Iter->GetFName(), *Iter);
	}

	// Animation Properties
	for (UAvaSequence* const Animation : GeneratedClass->Animations)
	{
		if (Animation)
		{
			// Find property with the same name as the animation and assign the animation to it.
			if (FObjectPropertyBase* const* Property = ObjectPropertyMap.Find(Animation->GetFName()))
			{
				check(*Property);
				(*Property)->SetObjectPropertyValue_InContainer(InActor, Animation);
			}
		}
	}
}
