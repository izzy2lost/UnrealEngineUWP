// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/PropertyAnimatorCoreWorldTimeSource.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Engine/World.h"

double UPropertyAnimatorCoreWorldTimeSource::GetTimeElapsed()
{
	if (const UWorld* World = GetWorld())
	{
		return World->GetTimeSeconds();
	}

	return 0.f;
}

bool UPropertyAnimatorCoreWorldTimeSource::IsTimeSourceReady() const
{
	const UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	return World->IsEditorWorld()
		|| World->IsPreviewWorld()
		|| (World->IsGameWorld() && World->HasBegunPlay());
}
