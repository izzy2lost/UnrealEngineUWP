// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationBackstopConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationBackstopConfigNode)

FChaosClothAssetSimulationBackstopConfigNode::FChaosClothAssetSimulationBackstopConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	if (FDataflowInput* const Input = RegisterInputConnection(&BackstopDistance.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&BackstopRadius.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
}

void FChaosClothAssetSimulationBackstopConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetPropertyWeighted(this, &BackstopDistance);
	PropertyHelper.SetPropertyWeighted(this, &BackstopRadius);
}
