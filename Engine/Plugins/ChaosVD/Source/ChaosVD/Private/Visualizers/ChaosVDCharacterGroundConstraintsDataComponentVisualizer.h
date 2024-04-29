// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "Chaos/Core.h"
#include "Components/ChaosVDSolverCharacterGroundConstraintDataComponent.h"

class AChaosVDSolverInfoActor;
enum class EChaosVDCharacterGroundConstraintDataVisualizationFlags : uint32;
struct FChaosVDCharacterGroundConstraintDebugDrawSettings;
struct FChaosVDCharacterGroundConstraint;

/** Visualization context structure specific for character ground constraint visualizations */
struct FChaosVDCharacterGroundConstraintVisualizationDataContext : public FChaosVDVisualizationContext
{
	FChaosVDCharacterGroundConstraintSelectionHandle DataSelectionHandle = FChaosVDCharacterGroundConstraintSelectionHandle(nullptr);
	
	AChaosVDSolverInfoActor* SolverInfoActor = nullptr;

	bool IsVisualizationFlagEnabled(EChaosVDCharacterGroundConstraintDataVisualizationFlags Flag) const;
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
	FChaosVDCharacterGroundConstraintDataComponentVisualizer();
	
	void RegisterVisualizerMenus();

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:

	void DrawConstraint(const UActorComponent* Component, const FChaosVDCharacterGroundConstraint& InConstraintData, FChaosVDCharacterGroundConstraintVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
};
