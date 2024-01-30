// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "SVGEngineSubsystem.generated.h"

class ASVGActor;
class ASVGShapesParentActor;

DECLARE_DELEGATE_OneParam(FSVGActorComponentsReady, ASVGActor*)
DECLARE_DELEGATE_OneParam(FOnSVGActorSplit, ASVGShapesParentActor*)

UCLASS()
class SVGIMPORTER_API USVGEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static USVGEngineSubsystem* Get();

	FSVGActorComponentsReady& GetSVGActorComponentsReadyDelegate() { return SVGActorComponentsReady; }

	static FOnSVGActorSplit& OnSVGActorSplit() { return OnSVGActorSplitDelegate; }

protected:
	static FOnSVGActorSplit OnSVGActorSplitDelegate;

	FSVGActorComponentsReady SVGActorComponentsReady;
};
