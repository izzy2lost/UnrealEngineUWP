// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationStretchConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationStretchConfigNode)

FChaosClothAssetSimulationStretchConfigNode::FChaosClothAssetSimulationStretchConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchStiffness.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchStiffnessWarp.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchStiffnessWeft.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchStiffnessBias.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchDamping.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchAnisoDamping.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchWarpScale.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&StretchWeftScale.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
	if (FDataflowInput* const Input = RegisterInputConnection(&AreaStiffness.WeightMap))
	{
		Input->SetCanHidePin(true);
		Input->SetPinIsHidden(true);
	}
}

void FChaosClothAssetSimulationStretchConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	if(SolverType == EChaosClothAssetConstraintSolverType::XPBD)
	{
		if (DistributionType == EChaosClothAssetConstraintDistributionType::Anisotropic)
		{
			PropertyHelper.SetPropertyBool(FName(TEXT("XPBDAnisoSpringUse3dRestLengths")), bStretchUse3dRestLengths, {
				FName(TEXT("XPBDAnisoStretchUse3dRestLengths"))}, ECollectionPropertyFlags::None);  // Non animatable

			PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessWarp")), StretchStiffnessWarp, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetStretchStiffness().Warp;
			 }, {
				FName(TEXT("EdgeSpringStiffness")),
				FName(TEXT("XPBDEdgeSpringStiffness")),
				FName(TEXT("XPBDAnisoStretchStiffnessWarp"))});

			PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessWeft")), StretchStiffnessWeft, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetStretchStiffness().Weft;
			 }, { FName(TEXT("XPBDAnisoStretchStiffnessWeft")) });
	
			PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringStiffnessBias")), StretchStiffnessBias, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetStretchStiffness().Bias;
			 }, { FName(TEXT("XPBDAnisoStretchStiffnessBias")) });

			PropertyHelper.SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoSpringDamping")), StretchAnisoDamping, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetDamping();
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
