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
	const FChaosVDConstraint* OwningConstraint = nullptr;
	int32 ContactIndex = INDEX_NONE;
};

struct HChaosVDContactPointProxy : public HHitProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDContactPointProxy(const FChaosVDCollisionDataFinder& InContactFinderData) : HHitProxy(HPP_UI), ContactFinder(InContactFinderData)
	{	
	}

	FChaosVDCollisionDataFinder ContactFinder;
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDCollisionVisualizationFlags: uint32
{
	None					= 0 UMETA(Hidden),
	ContactPoints			= 1 << 0,
	ContactInfo				= 1 << 1 UMETA(Hidden), // Not used yet
	NetPushOut				= 1 << 2,
	NetImpulse				= 1 << 3,
	ContactNormal			= 1 << 4,
	AccumulatedImpulse		= 1 << 5,
	DrawOnlyActiveContacts	= 1 << 6,
	EnableDrawForAllParticles	= 1 << 7,
};
ENUM_CLASS_FLAGS(EChaosVDCollisionVisualizationFlags);

class FChaosVDSolverCollisionDataComponentVisualizer : public FComponentVisualizer
{
public:
	FChaosVDSolverCollisionDataComponentVisualizer();
	~FChaosVDSolverCollisionDataComponentVisualizer();

	virtual bool ShowWhenSelected() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

protected:

	void DrawnMidPhaseData(const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhase, const FChaosVDVisualizationContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
};
