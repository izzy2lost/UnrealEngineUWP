// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationAerodynamicsConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationAerodynamicsConfigNode)

FChaosClothAssetSimulationAerodynamicsConfigNode::FChaosClothAssetSimulationAerodynamicsConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&Drag.WeightMap);
	RegisterInputConnection(&Lift.WeightMap);
}

void FChaosClothAssetSimulationAerodynamicsConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetProperty(this, &FluidDensity);
	PropertyHelper.SetPropertyWeighted(this, &Drag);
	PropertyHelper.SetPropertyWeighted(this, &Lift);
	PropertyHelper.SetProperty(this, &WindVelocity);
}
