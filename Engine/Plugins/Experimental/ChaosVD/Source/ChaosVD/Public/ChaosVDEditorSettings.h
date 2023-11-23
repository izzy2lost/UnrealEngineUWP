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

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDSettingChaged, UChaosVDEditorSettings* CVDEditorSettingsObject)

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
	ShowHeightfields = 1 << 5, // Selecting this will show heightfields even if complex is not selected
	ShowDisabledParticles = 1 << 6,
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
};

/** Structure holding the settings using to debug draw Particles shape based on their state on the Chaos Visual Debugger */
USTRUCT()
struct FChaosDebugDrawColorsByState
{
	GENERATED_BODY()

	/** Color used for dynamic particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor DynamicColor = FColor(255, 255, 0);
	
	/** Color used for sleeping particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor SleepingColor = FColor(128, 128, 128);

	/** Color used for kinematic particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor KinematicColor = FColor(0, 128, 255);

	/** Color used for static particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor StaticColor = FColor(255, 0, 0);

	FColor GetColorFromState(EChaosVDObjectStateType State) const;
};

/** Structure holding the settings using to debug draw Particles shape based on their shape type on the Chaos Visual Debugger */
USTRUCT()
struct FChaosDebugDrawColorsByShapeType
{
	GENERATED_BODY()

	/** Color used for Sphere, Plane, Cube, Capsule, Cylinder, tapered shapes */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor SimpleTypeColor = FColor(0, 255, 0); 

	/** Color used for convex shapes */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor ConvexColor = FColor(0, 255, 255);

	/** Color used for heightfield */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor HeightFieldColor = FColor(0, 0, 255);
	
	/** Color used for triangle meshes */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor TriangleMeshColor = FColor(255, 0, 0);

	/** Color used for triangle LevelSets */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor LevelSetColor = FColor(255, 0, 128);

	FColor GetColorFromShapeType(Chaos::EImplicitObjectType ShapeType) const;
};

/** Structure holding the settings using to debug draw Particles shape based on whether they are client or server objects (in PIE) Chaos Visual Debugger */
USTRUCT()
struct FChaosDebugDrawColorsByClientServer
{
	GENERATED_BODY()

	/** Color used for server shapes that are not awake or sleeping dynamic */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor ServerColor = FColor(50, 0, 0); 

	/** Color used for server shapes that are awake dynamic */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ServerDynamicColor = FColor(150, 0, 0);

	/** Color used for server shapes that are sleeping dynamics */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ServerSleepingColor = FColor(10, 0, 0);

	/** Color used for client shapes that are not awake or sleeping dynamic */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ClientColor = FColor(0, 0, 50);

	/** Color used for server shapes that are awake dynamic */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ClientDynamicColor = FColor(0, 0, 150);

	/** Color used for client shapes that are sleeping dynamics */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ClientSleepingColor = FColor(0, 0, 100);

	FColor GetColorFromState(bool bIsServer, EChaosVDObjectStateType State) const;
};

UENUM()
enum class EChaosVDParticleDebugColorMode
{
	None,
	State,
	ShapeType,
	ClientServer,
};

UCLASS(config = Engine)
class UChaosVDEditorSettings : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Viewport")
	float FarClippingOverride = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDParticleDataVisualizationFlags"))
	uint32 GlobalParticleDataVisualizationFlags = 0;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDCollisionVisualizationFlags"))
	uint32 GlobalCollisionDataVisualizationFlags = 0;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization")
	bool bShowDebugText = false;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization",  meta=(EditCondition = "GlobalCollisionDataVisualizationFlags != 0", EditConditionHides))
	FChaosVDContactDebugDrawSettings ContactDebugDrawSettings;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization")
	EChaosVDParticleDebugColorMode ParticleColorMode;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization", meta=(EditCondition = "ParticleColorMode == EChaosVDParticleDebugColorMode::ShapeType", EditConditionHides))
	FChaosDebugDrawColorsByShapeType ColorsByShapeType;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization", meta=(EditCondition = "ParticleColorMode == EChaosVDParticleDebugColorMode::State", EditConditionHides))
	FChaosDebugDrawColorsByState ColorsByParticleState;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization", meta = (EditCondition = "ParticleColorMode == EChaosVDParticleDebugColorMode::ClientServer", EditConditionHides))
	FChaosDebugDrawColorsByClientServer ColorsByClientServer;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking")
	EChaosVDActorTrackingTarget TrackingTarget;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	EChaosVDActorTrackingMode TrackingOptions;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingOptions == EChaosVDActorTrackingMode::ByDistanceOffset && TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	float TrackingDistanceOffset = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingOptions == EChaosVDActorTrackingMode::ByBoundingBox && TrackingTarget != EChaosVDActorTrackingTarget::Disabled", EditConditionHides))
	float ExpandViewTrackingBy = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Geometry Visibility", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDGeometryVisibilityFlags"))
	uint8 GeometryVisibilityFlags = static_cast<uint8>(EChaosVDGeometryVisibilityFlags::Simulated | EChaosVDGeometryVisibilityFlags::Simple |  EChaosVDGeometryVisibilityFlags::ShowHeightfields);

	UPROPERTY(Config)
	TSoftObjectPtr<UMaterial> QueryOnlyMeshesMaterial;

	UPROPERTY(Config)
	TSoftObjectPtr<UMaterial> SimOnlyMeshesMaterial;

	UPROPERTY(Config)
	FSoftClassPath SkySphereActorClass;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FChaosVDSettingChaged& OnVisibilitySettingsChanged() { return VisibilitySettingsChangedDelegate; }

	FChaosVDSettingChaged& OnColorSettingsChanged() { return ColorsSettingsChangedDelegate; }

	FChaosVDSettingChaged& OnFarClippingOverrideChanged() { return FarClippingOverrideChangedDelegate; }

	TSharedPtr<FName> SelectedTrackedTransformName;
	TSharedPtr<FName> SelectedTrackedLocationName;

protected:
	FChaosVDSettingChaged VisibilitySettingsChangedDelegate;
	FChaosVDSettingChaged ColorsSettingsChangedDelegate;
	FChaosVDSettingChaged FarClippingOverrideChangedDelegate;
};
