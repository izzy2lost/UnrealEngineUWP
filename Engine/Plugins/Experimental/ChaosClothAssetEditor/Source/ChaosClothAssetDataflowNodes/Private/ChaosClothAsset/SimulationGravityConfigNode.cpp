// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationGravityConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationGravityConfigNode)

FChaosClothAssetSimulationGravityConfigNode::FChaosClothAssetSimulationGravityConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationGravityConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyBool(this, &bUseGravityOverride);
	PropertyHelper.SetProperty(this, &GravityScale);
	PropertyHelper.SetProperty(this, &GravityOverride);
}
