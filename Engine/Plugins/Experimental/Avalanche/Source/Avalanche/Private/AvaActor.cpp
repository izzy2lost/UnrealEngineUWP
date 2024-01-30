// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaActor.h"
#include "AvaBlueprintGeneratedClass.h"
#include "AvaSequencePlayer.h"

AAvalancheActor::AAvalancheActor()
{
}

bool AAvalancheActor::UpdateGeneratedClass(bool bForceUpdate)
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

void AAvalancheActor::PostActorCreated()
{
	Super::PostActorCreated();
	UpdateGeneratedClass();
}

void AAvalancheActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	UpdateGeneratedClass();
}
