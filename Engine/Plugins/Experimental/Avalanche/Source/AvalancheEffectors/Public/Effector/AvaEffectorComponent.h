// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "AvaEffectorComponent.generated.h"

/** Class used to define a custom visualizer for this component and the effector actor */
UCLASS(MinimalAPI, Within = AvaEffectorActor)
class UAvaEffectorComponent : public USceneComponent
{
	GENERATED_BODY()
};