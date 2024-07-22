// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "PrimitiveComponentId.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "AvaTransitionRenderingSubsystem.generated.h"

class FSceneView;
class ULevel;

UCLASS(MinimalAPI)
class UAvaTransitionRenderingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	AVALANCHETRANSITION_API void ShowLevel(TObjectKey<ULevel> InLevel);
	AVALANCHETRANSITION_API void HideLevel(TObjectKey<ULevel> InLevel);

	void SetupView(FSceneView& InView);

private:
	TSet<TObjectKey<ULevel>> HiddenLevels;

	TSet<FPrimitiveComponentId> HiddenPrimitives;
};
