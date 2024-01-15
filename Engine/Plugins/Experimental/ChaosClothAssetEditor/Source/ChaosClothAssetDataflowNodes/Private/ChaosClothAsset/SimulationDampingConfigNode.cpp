// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationDampingConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationDampingConfigNode)

FChaosClothAssetSimulationDampingConfigNode::FChaosClothAssetSimulationDampingConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationDampingConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetProperty(this, &DampingCoefficient);
	PropertyHelper.SetProperty(this, &LocalDampingCoefficient);
}
