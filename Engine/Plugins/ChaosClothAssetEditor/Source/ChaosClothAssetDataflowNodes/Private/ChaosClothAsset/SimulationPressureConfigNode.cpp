// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationPressureConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationPressureConfigNode)

FChaosClothAssetSimulationPressureConfigNode::FChaosClothAssetSimulationPressureConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	if (FDataflowInput* const Input = RegisterInputConnection(&Pressure.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
}

void FChaosClothAssetSimulationPressureConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("Pressure")), Pressure, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
	{
		return FabricFacade.GetPressure();
	}, {});
}
