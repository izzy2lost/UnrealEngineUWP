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
	RegisterInputConnection(&StretchWarpScale.WeightMap);
	RegisterInputConnection(&StretchWeftScale.WeightMap);
	RegisterInputConnection(&AreaStiffness.WeightMap);
}

void FChaosClothAssetSimulationStretchConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(PropertyHelper.GetClothCollection());
	if(!CanUseFabrics(ClothFacade))
	{
		if(SolverType == EChaosClothAssetConstraintSolverType::XPBD)
		{
			if(DistributionType == EChaosClothAssetConstraintDistributionType::Anisotropic)
			{
				if(ConstraintType == EChaosClothAssetStretchConstraintType::StretchShear)
				{
					PropertyHelper.SetPropertyBool(FName(TEXT("XPBDAnisoStretchUse3dRestLengths")), bStretchUse3dRestLengths, {
						FName(TEXT("XPBDAnisoSpringUse3dRestLengths"))}, ECollectionPropertyFlags::None);  // Non animatable

					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchStiffnessWarp")), StretchStiffnessWarp, {
						FName(TEXT("EdgeSpringStiffness")),
						FName(TEXT("XPBDEdgeSpringStiffness")),
						FName(TEXT("XPBDAnisoSpringStiffnessWarp"))});
				
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchStiffnessWeft")), StretchStiffnessWeft, {
						FName(TEXT("XPBDAnisoSpringStiffnessWeft"))});
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchStiffnessBias")), StretchStiffnessBias, {
						FName(TEXT("XPBDAnisoSpringStiffnessBias"))});
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchDamping")), StretchDamping, {
						FName(TEXT("XPBDEdgeSpringDamping")),
						FName(TEXT("XPBDAnisoSpringDamping"))});
				
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchWarpScale")), StretchWarpScale, {
						FName(TEXT("XPBDAnisoSpringWarpScale"))});
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchWeftScale")), StretchWeftScale, {
						FName(TEXT("XPBDAnisoSpringWeftScale"))});
				}
				else
				{
					PropertyHelper.SetPropertyBool(FName(TEXT("XPBDAnisoSpringUse3dRestLengths")), bStretchUse3dRestLengths, {
						FName(TEXT("XPBDAnisoStretchUse3dRestLengths"))}, ECollectionPropertyFlags::None);  // Non animatable

					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessWarp")), StretchStiffnessWarp, {
						FName(TEXT("EdgeSpringStiffness")),
						FName(TEXT("XPBDEdgeSpringStiffness")),
						FName(TEXT("XPBDAnisoStretchStiffnessWarp"))});
				
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessWeft")), StretchStiffnessWeft, {
						FName(TEXT("XPBDAnisoStretchStiffnessWeft"))});
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessBias")), StretchStiffnessBias, {
						FName(TEXT("XPBDAnisoStretchStiffnessBias"))});
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringDamping")), StretchDamping, {
						FName(TEXT("XPBDAnisoStretchDamping")),
						FName(TEXT("XPBDEdgeSpringDamping"))});
				
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringWarpScale")), StretchWarpScale, {
						FName(TEXT("XPBDAnisoStretchWarpScale"))});
					PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoSpringWeftScale")), StretchWeftScale, {
						FName(TEXT("XPBDAnisoStretchWeftScale"))});
				}
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
	else
	{
		PropertyHelper.SetPropertyBool(FName(TEXT("XPBDAnisoStretchUse3dRestLengths")), bStretchUse3dRestLengths, {
						FName(TEXT("XPBDAnisoSpringUse3dRestLengths"))}, ECollectionPropertyFlags::None);  // Non animatable

		SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoStretchStiffnessWarp")), StretchStiffnessWarp,ClothFacade, PropertyHelper, [](
			const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetXPBDAnisoStretchStiffness().Warp;
		 }, {
			FName(TEXT("EdgeSpringStiffness")),
			FName(TEXT("XPBDEdgeSpringStiffness"))});

		SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoStretchStiffnessWeft")), StretchStiffnessWeft, ClothFacade, PropertyHelper, [](
			const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetXPBDAnisoStretchStiffness().Weft;
		 }, {});
		
		SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoStretchStiffnessBias")), StretchStiffnessBias,ClothFacade, PropertyHelper, [](
			const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetXPBDAnisoStretchStiffness().Bias;
		 }, {});

		SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoStretchDamping")), StretchDamping,ClothFacade, PropertyHelper, [](
			const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
		 {
			 return FabricFacade.GetXPBDAnisoDamping();
		 }, {
			FName(TEXT("XPBDEdgeSpringDamping"))});

		PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchWarpScale")), StretchWarpScale, {});
		PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoStretchWeftScale")), StretchWeftScale, {});
	}
}
