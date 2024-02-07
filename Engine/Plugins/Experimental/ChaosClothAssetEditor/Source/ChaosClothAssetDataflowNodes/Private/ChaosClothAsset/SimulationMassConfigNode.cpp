// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationMassConfigNode.h"
#include "ChaosClothAsset/SimulationFabricConfigNode.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "UObject/FortniteValkyrieBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationMassConfigNode)

FChaosClothAssetSimulationMassConfigNode::FChaosClothAssetSimulationMassConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationFabricConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&UniformMassWeighted.WeightMap);
	RegisterInputConnection(&DensityWeighted.WeightMap);
}

void FChaosClothAssetSimulationMassConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(PropertyHelper.GetClothCollection());
	if(!CanUseFabrics(ClothFacade))
	{
		PropertyHelper.SetPropertyEnum(this, &MassMode, {}, ECollectionPropertyFlags::Intrinsic);
		switch (MassMode)
		{
		default:
		case EClothMassMode::UniformMass:
			{
				PropertyHelper.SetPropertyWeighted(FName(TEXT("MassValue")), UniformMassWeighted, {}, ECollectionPropertyFlags::Intrinsic);
			}
			break;
		case EClothMassMode::TotalMass:
			{
				PropertyHelper.SetProperty(FName(TEXT("MassValue")), TotalMass, {}, ECollectionPropertyFlags::Intrinsic);
			}
			break;
		case EClothMassMode::Density:
			{
				PropertyHelper.SetPropertyWeighted(FName(TEXT("MassValue")), DensityWeighted, {}, ECollectionPropertyFlags::Intrinsic);
			}
			break;
		}
	}
	else
	{
		constexpr EClothMassMode MassModeProperty = EClothMassMode::Density;
		PropertyHelper.SetPropertyEnum(FName(TEXT("MassMode")), MassModeProperty, {}, ECollectionPropertyFlags::Intrinsic);

		SetFabricPropertyWeighted(FName(TEXT("MassValue")), DensityWeighted, ClothFacade, PropertyHelper, [](const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetDensityWeighted();
		 }, {}, ECollectionPropertyFlags::Intrinsic);
	}
	PropertyHelper.SetProperty(this, &MinPerParticleMass, {}, ECollectionPropertyFlags::Intrinsic);
}

void FChaosClothAssetSimulationMassConfigNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FFortniteValkyrieBranchObjectVersion::GUID);
	if (Ar.IsLoading())
	{
#if WITH_EDITORONLY_DATA
		if (Ar.CustomVer(FFortniteValkyrieBranchObjectVersion::GUID) < FFortniteValkyrieBranchObjectVersion::ChaosClothAssetWeightedMassAndGravity)
		{
			UniformMassWeighted.Low = UniformMassWeighted.High = UniformMass_DEPRECATED;
			DensityWeighted.Low = DensityWeighted.High = Density_DEPRECATED;
		}
#endif
	}
}
