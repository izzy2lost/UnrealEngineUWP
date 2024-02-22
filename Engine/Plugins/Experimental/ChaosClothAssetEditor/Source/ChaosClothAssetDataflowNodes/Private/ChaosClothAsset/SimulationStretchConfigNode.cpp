// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationStretchConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationStretchConfigNode)

FChaosClothAssetSimulationStretchConfigNode::FChaosClothAssetSimulationStretchConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationFabricConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&StretchStiffness.WeightMap);
	RegisterInputConnection(&StretchStiffnessWarp.WeightMap);
	RegisterInputConnection(&StretchStiffnessWeft.WeightMap);
	RegisterInputConnection(&StretchStiffnessBias.WeightMap);
	RegisterInputConnection(&StretchDamping.WeightMap);
	RegisterInputConnection(&StretchAnisoDamping.WeightMap);
	RegisterInputConnection(&StretchWarpScale.WeightMap);
	RegisterInputConnection(&StretchWeftScale.WeightMap);
	RegisterInputConnection(&AreaStiffness.WeightMap);
}

void FChaosClothAssetSimulationStretchConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(PropertyHelper.GetClothCollection());
	if(!ClothFacade.IsValid())
	{
		return;
	}
	
	if(SolverType == EChaosClothAssetConstraintSolverType::XPBD)
	{
		if (DistributionType == EChaosClothAssetConstraintDistributionType::Anisotropic)
		{
			PropertyHelper.SetPropertyBool(FName(TEXT("XPBDAnisoSpringUse3dRestLengths")), bStretchUse3dRestLengths, {
				FName(TEXT("XPBDAnisoStretchUse3dRestLengths"))}, ECollectionPropertyFlags::None);  // Non animatable

			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessWarp")), StretchStiffnessWarp,ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoSpringStiffness().Warp;
			 }, {
				FName(TEXT("EdgeSpringStiffness")),
				FName(TEXT("XPBDEdgeSpringStiffness")),
				FName(TEXT("XPBDAnisoStretchStiffnessWarp"))});

			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessWeft")), StretchStiffnessWeft, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoSpringStiffness().Weft;
			 }, { FName(TEXT("XPBDAnisoStretchStiffnessWeft")) });
	
			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessBias")), StretchStiffnessBias, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoSpringStiffness().Bias;
			 }, { FName(TEXT("XPBDAnisoStretchStiffnessBias")) });

			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringDamping")), StretchAnisoDamping, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoDamping();
			 }, { FName(TEXT("XPBDEdgeSpringDamping")),
				FName(TEXT("XPBDAnisoStretchDamping")) });

			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringWarpScale")), StretchWarpScale, { FName(TEXT("XPBDAnisoStretchWarpScale")) });
			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringWeftScale")), StretchWeftScale, { FName(TEXT("XPBDAnisoStretchWeftScale")) });
		}
		else
		{
			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDEdgeSpringStiffness")), StretchStiffness, {
				FName(TEXT("EdgeSpringStiffness")),
				FName(TEXT("XPBDAnisoStretchStiffnessWarp")),
				FName(TEXT("XPBDAnisoSpringStiffnessWarp"))});

			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDEdgeSpringDamping")), StretchDamping, {
				FName(TEXT("XPBDAnisoStretchDamping")),
				FName(TEXT("XPBDAnisoSpringDamping"))});

			if(bAddAreaConstraint)
			{
				PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAreaSpringStiffness")), AreaStiffness,{
					FName(TEXT("AreaSpringStiffness"))});
			}
		}
	}
	else
	{
		PropertyHelper.SetPropertyWeighted(FName(TEXT("EdgeSpringStiffness")), StretchStiffness, {
			FName(TEXT("XPBDEdgeSpringStiffness")), 
			FName(TEXT("XPBDAnisoStretchStiffnessWarp")),
			FName(TEXT("XPBDAnisoSpringStiffnessWarp"))});
		
		if(bAddAreaConstraint)
		{
			PropertyHelper.SetPropertyWeighted(FName(TEXT("AreaSpringStiffness")), AreaStiffness,{
				FName(TEXT("XPBDAreaSpringStiffness"))});
		}
	}
}
