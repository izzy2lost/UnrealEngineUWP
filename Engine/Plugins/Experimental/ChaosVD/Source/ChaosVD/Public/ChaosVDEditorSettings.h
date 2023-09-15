// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Visualizers/ChaosVDParticleDataVisualizer.h"
#include "Visualizers/ChaosVDSolverCollisionDataComponentVisualizer.h"

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

/** Structure holding the settings using to debug draw contact data on the Chaos Visual Debugger */
USTRUCT()
struct FChaosVDContactDebugDrawSettings
{
	GENERATED_BODY()

	/** The depth priority used for while drawing contact data. Can be World or Foreground (with this one the shapes will be drawn on top of the geometry and be always visible) */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	TEnumAsByte<ESceneDepthPriorityGroup> DepthPriority = ESceneDepthPriorityGroup::SDPG_World;

	/** The radius of the debug draw circle used to represent a contact point */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float ContactCircleRadius = 6.0f;

	/** The scale value to be applied to the normal vector of a contact used to change its size to make it easier to see */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float ContactNormalScale = 30.0f;

	/** The radius of the debug draw circle used to represent the Phi value (penetration) of a contact point */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float ContactPhiCircleRadius = 2.0f;
};

UCLASS(config = Engine)
class UChaosVDEditorSettings : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDParticleDataVisualizationFlags"))
	uint32 GlobalParticleDataVisualizationFlags = 0;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDCollisionVisualizationFlags"))
	uint32 GlobalCollisionDataVisualizationFlags = 0;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization")
	bool bShowDebugText = false;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization",  meta=(EditCondition = "GlobalCollisionDataVisualizationFlags != 0", EditConditionHides))
	FChaosVDContactDebugDrawSettings ContactDebugDrawSettings;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking")
	EChaosVDActorTrackingTarget TrackingTarget;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	EChaosVDActorTrackingMode TrackingOptions;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingOptions == EChaosVDActorTrackingMode::ByDistanceOffset && TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	float TrackingDistanceOffset = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingOptions == EChaosVDActorTrackingMode::ByBoundingBox && TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	float ExpandViewTrackingBy = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Geometry Visibility", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDGeometryVisibilityFlags"))
	uint8 GeometryVisibilityFlags = static_cast<uint8>(EChaosVDGeometryVisibilityFlags::Simulated | EChaosVDGeometryVisibilityFlags::Simple);

	UPROPERTY(Config)
	TSoftObjectPtr<UMaterial> QueryOnlyMeshesMaterial;

	UPROPERTY(Config)
	FSoftClassPath SkySphereActorClass;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FChaosVDVisibilitySettingsChaged& OnVisibilitySettingsChanged() { return VisibilitySettingsChangedDelegate; }

	TSharedPtr<FName> SelectedTrackedTransformName;
	TSharedPtr<FName> SelectedTrackedLocationName;

protected:
	FChaosVDVisibilitySettingsChaged VisibilitySettingsChangedDelegate;
};
