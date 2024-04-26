// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "Chaos/Core.h"
#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"

struct FChaosVDCharacterGroundConstraintDebugDrawSettings;
class AChaosVDSolverInfoActor;
struct FChaosVDCharacterGroundConstraint;

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDCharacterGroundConstraintDataVisualizationFlags : uint32
{
	None					= 0 UMETA(Hidden),
	/** Draw the target movement vector */
	TargetDeltaPosition		= 1 << 0,
	/** Draw the target orientation facing vector */
	TargetDeltaFacing		= 1 << 1,
	/** Draw the ground query distance based on the constraint's data */
	GroundQueryDistance		= 1 << 2,
	/** Draw the ground query normal based on the constraint's data */
	GroundQueryNormal		= 1 << 3,
	/** Draw the applied force vector */
	AppliedRadialForce		= 1 << 4,
	/** Draw the applied force vector */
	AppliedNormalForce		= 1 << 5,
	/** Draw the applied force vector */
	AppliedTorque			= 1 << 6,
	/** Draw the constraint even if it is disabled */
	DrawDisabled			= 1 << 7,
	/** Only debugs draw data for a selected constraint */
	OnlyDrawSelected		= 1 << 8,
	/** Enables debug draw for constraint data from any solver */
	EnableDraw				= 1 << 9,
};
ENUM_CLASS_FLAGS(EChaosVDCharacterGroundConstraintDataVisualizationFlags);

/** Visualization context structure specific for character ground constraint visualizations */
struct FChaosVDCharacterGroundConstraintVisualizationDataContext : public FChaosVDVisualizationContext
{
	FChaosVDCharacterGroundConstraintSelectionHandle DataSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle(nullptr);
	bool bIsServerVisualizationEnabled = false;
	
	AChaosVDSolverInfoActor* SolverInfoActor = nullptr;

	const FChaosVDCharacterGroundConstraintDebugDrawSettings* DebugDrawSettings = nullptr;

	bool bShowDebugText = false;

	bool IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags Flag) const
	{
		const EChaosVDCharacterGroundConstraintDataVisualizationFlags FlagsAsParticleFlags = static_cast<EChaosVDCharacterGroundConstraintDataVisualizationFlags>(VisualizationFlags);
		return EnumHasAnyFlags(FlagsAsParticleFlags, Flag);
	}
};

/** Custom Hit Proxy for debug drawn scene queries */
struct HChaosVDCharacterGroundConstraintProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDCharacterGroundConstraintProxy(const UActorComponent* Component, const FChaosVDCharacterGroundConstraintSelectionHandle& InConstraintFinderData) : HComponentVisProxy(Component, HPP_UI), DataSelectionHandle(InConstraintFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDCharacterGroundConstraintSelectionHandle DataSelectionHandle;
};

/**
 * Component visualizer in charge of generating debug draw visualizations for character ground constraints in a UChaosVDSolverCharacterGroundConstraintDataComponent
 */
class FChaosVDCharacterGroundConstraintDataComponentVisualizer final : public FComponentVisualizer
{
public:
	FChaosVDCharacterGroundConstraintDataComponentVisualizer()
	{
	}

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:

	void DrawConstraint(const UActorComponent* Component, const FChaosVDCharacterGroundConstraint& InConstraintData, FChaosVDCharacterGroundConstraintVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
};
