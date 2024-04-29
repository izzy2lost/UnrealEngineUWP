// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"
#include "HitProxies.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

struct FChaosVDParticlePairMidPhase;
struct FChaosVDVisualizationContext;
class FChaosVDScene;

struct FChaosVDCollisionDataFinder
{
	TWeakPtr<FChaosVDParticlePairMidPhase> OwningMidPhase;
	FChaosVDConstraint* OwningConstraint = nullptr;
	int32 ContactIndex = INDEX_NONE;

	void SetIsSelected(bool bNewSelected);
};

struct HChaosVDContactPointProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDContactPointProxy(const UActorComponent* Component, const FChaosVDCollisionDataFinder& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), ContactFinder(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDCollisionDataFinder ContactFinder;
};

class FChaosVDSolverCollisionDataComponentVisualizer : public FComponentVisualizer
{
public:
	
	FChaosVDSolverCollisionDataComponentVisualizer();
	virtual ~FChaosVDSolverCollisionDataComponentVisualizer() override;
	
	void RegisterVisualizerMenus();

	virtual bool ShowWhenSelected() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:
	
	void ClearCurrentSelection();

	void DrawMidPhaseData(const UActorComponent* Component, const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhase, const FChaosVDVisualizationContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);

	FChaosVDCollisionDataFinder CurrentSelectedContactData;
};
