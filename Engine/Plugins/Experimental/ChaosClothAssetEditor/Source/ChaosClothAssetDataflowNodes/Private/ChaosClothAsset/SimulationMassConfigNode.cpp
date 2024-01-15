// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationMassConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationMassConfigNode)

FChaosClothAssetSimulationMassConfigNode::FChaosClothAssetSimulationMassConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
}

void FChaosClothAssetSimulationMassConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	float MassValue;
	switch (MassMode)
	{
	default:
	case EClothMassMode::UniformMass: MassValue = UniformMass; break;
	case EClothMassMode::TotalMass: MassValue = TotalMass; break;
	case EClothMassMode::Density: MassValue = Density; break;
	}

	PropertyHelper.SetPropertyEnum(this, &MassMode, {}, ECollectionPropertyFlags::Intrinsic);
	PropertyHelper.SetProperty(FName(TEXT("MassValue")), MassValue, {}, ECollectionPropertyFlags::Intrinsic);
	PropertyHelper.SetProperty(this, &MinPerParticleMass, {}, ECollectionPropertyFlags::Intrinsic);
}
