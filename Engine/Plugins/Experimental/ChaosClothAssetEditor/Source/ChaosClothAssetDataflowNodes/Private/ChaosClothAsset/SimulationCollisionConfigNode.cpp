// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationCollisionConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationCollisionConfigNode)

FChaosClothAssetSimulationCollisionConfigNode::FChaosClothAssetSimulationCollisionConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationFabricConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationCollisionConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(PropertyHelper.GetClothCollection());
	if(!CanUseFabrics(ClothFacade))
	{
		PropertyHelper.SetProperty(this, &FrictionCoefficient);
		PropertyHelper.SetProperty(this, &CollisionThickness);
	}
	else
	{
		SetFabricProperty(FName(TEXT("FrictionCoefficient")), ClothFacade, PropertyHelper, [](const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetFrictionCoefficient();
		 }, {});
		SetFabricProperty(FName(TEXT("CollisionThickness")), ClothFacade, PropertyHelper, [](const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetCollisionThickness();
		 }, {});
	}
	PropertyHelper.SetPropertyBool(this, &bUseCCD);
	PropertyHelper.SetProperty(this, &ProximityStiffness);
}
