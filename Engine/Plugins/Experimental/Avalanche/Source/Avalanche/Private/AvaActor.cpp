// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaActor.h"
#include "AvaBlueprintGeneratedClass.h"
#include "AvaSequencePlayer.h"

AAvaActor::AAvaActor()
{
}

bool AAvaActor::UpdateGeneratedClass(bool bForceUpdate)
{
	if ((bGeneratedClassUpdated && !bForceUpdate) || HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	UAvaBlueprintGeneratedClass* GeneratedClass = Cast<UAvaBlueprintGeneratedClass>(GetClass());
	if (GeneratedClass)
	{
		GeneratedClass->UpdateProperties(this);
	}

	bGeneratedClassUpdated = true;
	return true;
}

void AAvaActor::PostActorCreated()
{
	Super::PostActorCreated();
	UpdateGeneratedClass();
}

#if WITH_EDITOR
FString AAvaActor::GetDefaultActorLabel() const
{
	return TEXT("Motion Design Actor");
}
#endif

void AAvaActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	UpdateGeneratedClass();
}
