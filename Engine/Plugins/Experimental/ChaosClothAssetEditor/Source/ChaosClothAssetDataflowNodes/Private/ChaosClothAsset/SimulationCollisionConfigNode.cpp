// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationCollisionConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationCollisionConfigNode)

FChaosClothAssetSimulationCollisionConfigNode::FChaosClothAssetSimulationCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationCollisionConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetProperty(this, &FrictionCoefficient);
	PropertyHelper.SetProperty(this, &CollisionThickness);
	
	PropertyHelper.SetPropertyBool(this, &bUseCCD);
	PropertyHelper.SetProperty(this, &ProximityStiffness);
}
