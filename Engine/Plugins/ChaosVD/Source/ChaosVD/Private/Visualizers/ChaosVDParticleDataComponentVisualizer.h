// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"
#include "HAL/Platform.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "Settings/ChaosVDParticleVisualizationSettings.h"
#include "Templates/SharedPointer.h"

class FChaosVDGeometryBuilder;
struct FChaosParticleDataDebugDrawSettings;

struct FChaosVDVisualizedParticleDataSelectionHandle
{
	int32 ParticleIndex = INDEX_NONE;
	int32 SolverID = INDEX_NONE;
};

class UChaosVDParticleVisualizationDebugDrawSettings;

struct FChaosVDParticleDataVisualizationContext : public FChaosVDVisualizationContext
{
	TWeakPtr<FChaosVDGeometryBuilder> GeometryGenerator = nullptr;
	bool bIsSelectedData = false;
	bool bShowDebugText = false;

	const UChaosVDParticleVisualizationDebugDrawSettings* DebugDrawSettings = nullptr;

	bool IsVisualizationFlagEnabled(EChaosVDParticleDataVisualizationFlags Flag) const
	{
		const EChaosVDParticleDataVisualizationFlags FlagsAsParticleFlags = static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlags);
		return EnumHasAnyFlags(FlagsAsParticleFlags, Flag);
	}
};

/** Custom Hit Proxy for debug drawn particle data */
struct HChaosVDParticleDataProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDParticleDataProxy(const UActorComponent* Component, const FChaosVDVisualizedParticleDataSelectionHandle& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), DataSelectionHandle(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDVisualizedParticleDataSelectionHandle DataSelectionHandle;
};

/**
 * Component visualizer in charge of generating debug draw visualizations for for particles
 */
class FChaosVDParticleDataComponentVisualizer : public FComponentVisualizer
{
public:
	FChaosVDParticleDataComponentVisualizer();

	void RegisterVisualizerMenus();

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:
	
	void DrawParticleVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& InVector, EChaosVDParticleDataVisualizationFlags VectorID, const FChaosVDParticleDataVisualizationContext& InVisualizationContext, float LineThickness);
	void DrawVisualizationForParticleData(const UActorComponent* Component, FPrimitiveDrawInterface* PDI, const FSceneView* View, const FChaosVDParticleDataVisualizationContext& InVisualizationContext, const FChaosVDParticleDataWrapper& InParticleDataViewer);
};
