// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"
#include "Chaos/Core.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "Settings/ChaosVDJointConstraintVisualizationSettings.h"

struct FChaosVDJointConstraint;

class AChaosVDSolverInfoActor;
class UChaosVDJointConstraintsVisualizationSettings;

/** Visualization context structure specific for Joints visualizations */
struct FChaosVDJointVisualizationDataContext : public FChaosVDVisualizationContext
{
	FChaosVDJointConstraintSelectionHandle DataSelectionHandle = FChaosVDJointConstraintSelectionHandle(nullptr);
	bool bIsServerVisualizationEnabled = false;
	
	AChaosVDSolverInfoActor* SolverInfoActor = nullptr;

	const UChaosVDJointConstraintsVisualizationSettings* DebugDrawSettings = nullptr;

	bool bShowDebugText = false;

	bool IsVisualizationFlagEnabled(EChaosVDJointsDataVisualizationFlags Flag) const
	{
		const EChaosVDJointsDataVisualizationFlags FlagsAsParticleFlags = static_cast<EChaosVDJointsDataVisualizationFlags>(VisualizationFlags);
		return EnumHasAnyFlags(FlagsAsParticleFlags, Flag);
	}
};

/** Custom Hit Proxy for debug drawn scene queries */
struct HChaosVDJointConstraintProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDJointConstraintProxy(const UActorComponent* Component, const FChaosVDJointConstraintSelectionHandle& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), DataSelectionHandle(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDJointConstraintSelectionHandle DataSelectionHandle;
};

/**
 * Component visualizer in charge of generating debug draw visualizations for Joint Constraints in a UChaosVDSolverJointConstraintDataComponent
 */
class FChaosVDJointConstraintsDataComponentVisualizer final : public FComponentVisualizer
{
public:
	FChaosVDJointConstraintsDataComponentVisualizer();
	
	void RegisterVisualizerMenus();

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:

	void DebugDrawAllAxis(const FChaosVDJointConstraint& InJointConstraintData, FChaosVDJointVisualizationDataContext& VisualizationContext, FPrimitiveDrawInterface* PDI, const float LineThickness, const FVector& InPosition, const Chaos::FMatrix33& InRotationMatrix, TConstArrayView<FLinearColor>);
	void DrawJointConstraint(const UActorComponent* Component, const FChaosVDJointConstraint& InJointConstraintData, FChaosVDJointVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
};
