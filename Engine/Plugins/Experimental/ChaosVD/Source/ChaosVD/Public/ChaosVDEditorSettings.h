// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "ChaosVDEditorSettings.generated.h"


UENUM()
enum class EChaosVDActorTrackingMode
{
	ByDistanceOffset,
	ByBoundingBox,
	MatchTransform
};

UENUM()
enum class EChaosVDActorTrackingTarget
{
	Disabled,
	SelectedObject,
	RecordedTransform,
	RecordedLocation,
};

UCLASS(config = Engine)
class UChaosVDEditorSettings : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDParticleDataVisualizationFlags"))
	uint8 GlobalParticleDataVisualizationFlags = 0;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDCollisionVisualizationFlags"))
	uint8 GlobalCollisionDataVisualizationFlags = 0;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDCollisionVisualizationFlags"))
	bool bShowDebugText = false;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking")
	EChaosVDActorTrackingTarget TrackingTarget;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	EChaosVDActorTrackingMode TrackingOptions;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingOptions == EChaosVDActorTrackingMode::ByDistanceOffset && TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	float TrackingDistanceOffset = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingOptions == EChaosVDActorTrackingMode::ByBoundingBox && TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	float ExpandViewTrackingBy = 60.0f;

	UPROPERTY(Config)
	TSoftObjectPtr<UWorld> BasePhysicsVDWorld;
};
