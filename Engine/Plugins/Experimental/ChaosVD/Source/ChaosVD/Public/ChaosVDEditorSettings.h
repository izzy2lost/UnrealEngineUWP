// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "ChaosVDEditorSettings.generated.h"

class UChaosVDEditorSettings;
class UMaterial;

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDVisibilitySettingsChaged, UChaosVDEditorSettings* CVDEditorSettingsObject)

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

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDGeometryVisibilityFlags : uint8
{
	Query = 1 << 1,
	Simulated = 1 << 2,
	Simple = 1 << 3,
	Complex = 1 << 4,
};
ENUM_CLASS_FLAGS(EChaosVDGeometryVisibilityFlags)

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

	UPROPERTY(EditAnywhere, Category = "Geometry Visibility", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDGeometryVisibilityFlags"))
	uint8 GeometryVisibilityFlags = static_cast<uint8>(EChaosVDGeometryVisibilityFlags::Simulated | EChaosVDGeometryVisibilityFlags::Simple);

	UPROPERTY(Config)
	TSoftObjectPtr<UMaterial> QueryOnlyMeshesMaterial;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FChaosVDVisibilitySettingsChaged& OnVisibilitySettingsChanged() { return VisibilitySettingsChangedDelegate; };

protected:
	FChaosVDVisibilitySettingsChaged VisibilitySettingsChangedDelegate;
};
