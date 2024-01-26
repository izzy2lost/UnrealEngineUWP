// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "UnrealClient.h"

#include "GeometryMaskWorldSubsystem.generated.h"

class FGeometryMaskSceneViewExtension;

/** Updates the canvases. */
UCLASS()
class UGeometryMaskWorldSubsystem
	: public UWorldSubsystem
{
	GENERATED_BODY()

protected:
	// ~Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// ~End USubsystem

private:
	TSharedPtr<FGeometryMaskSceneViewExtension> GeometryMaskSceneViewExtension;
};
