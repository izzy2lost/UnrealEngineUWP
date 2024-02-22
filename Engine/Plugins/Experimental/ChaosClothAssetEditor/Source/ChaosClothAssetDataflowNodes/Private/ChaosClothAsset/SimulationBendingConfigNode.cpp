// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationBendingConfigNode.h"
#include "Chaos/CollectionPropertyFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationBendingConfigNode)

FChaosClothAssetSimulationBendingConfigNode::FChaosClothAssetSimulationBendingConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationFabricConfigNode(InParam, InGuid)
{
	RegisterCollectionConnections();
	RegisterInputConnection(&FlatnessRatio.WeightMap);
	RegisterInputConnection(&RestAngle.WeightMap);
	RegisterInputConnection(&BendingStiffness.WeightMap);
	RegisterInputConnection(&BendingStiffnessWarp.WeightMap);
	RegisterInputConnection(&BendingStiffnessWeft.WeightMap);
	RegisterInputConnection(&BendingStiffnessBias.WeightMap);
	RegisterInputConnection(&BendingDamping.WeightMap);
	RegisterInputConnection(&BendingAnisoDamping.WeightMap);
	RegisterInputConnection(&BucklingStiffness.WeightMap);
	RegisterInputConnection(&BucklingStiffnessWarp.WeightMap);
	RegisterInputConnection(&BucklingStiffnessWeft.WeightMap);
	RegisterInputConnection(&BucklingStiffnessBias.WeightMap);
}

void FChaosClothAssetSimulationBendingConfigNode::AddProperties(FPropertyHelper& PropertyHelper) const
{
	UE::Chaos::ClothAsset::FCollectionClothFacade ClothFacade(PropertyHelper.GetClothCollection());
	if(!ClothFacade.IsValid())
	{
		return;
	}
	
	if(SolverType == EChaosClothAssetConstraintSolverType::XPBD)
	{
		if(DistributionType == EChaosClothAssetConstraintDistributionType::Isotropic)
		{
			if(ConstraintType == EChaosClothAssetBendingConstraintType::FacesSpring)
			{
				PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDBendingSpringStiffness")), BendingStiffness,{
					FName(TEXT("XPBDBendingElementStiffness")),
					FName(TEXT("XPBDAnisoBendingStiffnessWarp")),
					FName(TEXT("BendingSpringStiffness")),
					FName(TEXT("BendingElementStiffness"))});

				PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDBendingSpringDamping")), BendingDamping,{
					FName(TEXT("XPBDBendingElementDamping")),
					FName(TEXT("XPBDAnisoBendingDamping"))});
			}
			else
			{
				PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDBendingElementStiffness")), BendingStiffness,{
					FName(TEXT("XPBDBendingSpringStiffness")),
					FName(TEXT("XPBDAnisoBendingStiffnessWarp")),
					FName(TEXT("BendingSpringStiffness")),
					FName(TEXT("BendingElementStiffness"))});

				PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDBendingElementDamping")), BendingDamping,{
					FName(TEXT("XPBDBendingSpringDamping")),
					FName(TEXT("XPBDAnisoBendingDamping"))});
				
				PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDBucklingStiffness")), BucklingStiffness,{
					FName(TEXT("XPBDAnisoBucklingStiffnessWarp")),
					FName(TEXT("BucklingStiffness"))});
				
				PropertyHelper.SetProperty(FName(TEXT("XPBDBucklingRatio")), BucklingRatio, {
					FName(TEXT("XPBDAnisoBucklingRatio")),
					FName(TEXT("BucklingRatios"))});
			}
			PropertyHelper.SetPropertyEnum(FName(TEXT("XPBDRestAngleType")), RestAngleType, {
				FName(TEXT("XPBDAnisoRestAngleType")),
				FName(TEXT("RestAngleType")) 
			}, ECollectionPropertyFlags::None);
			
			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDFlatnessRatio")), FlatnessRatio, {
				FName(TEXT("XPBDAnisoFlatnessRatio")),
				FName(TEXT("FlatnessRatio"))       
			});
			
			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDRestAngle")), RestAngle, {
				FName(TEXT("XPBDAnisoRestAngle")),
				FName(TEXT("RestAngle"))       
			});
		}
		else
		{
			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBendingStiffnessWarp")), BendingStiffnessWarp, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoBendingStiffness().Warp;
			 }, {
				FName(TEXT("BendingSpringStiffness")),     
				FName(TEXT("BendingElementStiffness")),     
				FName(TEXT("XPBDBendingSpringStiffness")),  
				FName(TEXT("XPBDBendingElementStiffness"))});

			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBendingStiffnessWeft")), BendingStiffnessWeft, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoBendingStiffness().Weft;
			 }, {});
			
			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBendingStiffnessBias")), BendingStiffnessBias,ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoBendingStiffness().Bias;
			 }, {});
			
			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBucklingStiffnessWarp")), BucklingStiffnessWarp,ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			 {
				 return FabricFacade.GetXPBDAnisoBucklingStiffness().Warp;
			 }, {
				FName(TEXT("BucklingStiffness")),    
				FName(TEXT("XPBDBucklingStiffness"))});
			
			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBucklingStiffnessWeft")), BucklingStiffnessWeft, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			{
				return FabricFacade.GetXPBDAnisoBucklingStiffness().Weft;
			},{});
			
			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBucklingStiffnessBias")), BucklingStiffnessBias, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			{
				return FabricFacade.GetXPBDAnisoBucklingStiffness().Bias;
			},{});

			SetFabricPropertyWeighted(FName(TEXT("XPBDAnisoBendingDamping")), BendingAnisoDamping, ClothFacade, PropertyHelper, [](
				const UE::Chaos::ClothAsset::FCollectionClothFabricFacade& FabricFacade)-> float
			{
				return FabricFacade.GetXPBDAnisoDamping();
			},{
				FName(TEXT("XPBDBendingSpringDamping")),  
				FName(TEXT("XPBDBendingElementDamping"))});

			PropertyHelper.SetProperty(FName(TEXT("XPBDAnisoBucklingRatio")), BucklingRatio, {
				FName(TEXT("BucklingRatio")),
				FName(TEXT("XPBDBucklingRatio"))});

			PropertyHelper.SetPropertyEnum(FName(TEXT("XPBDAnisoRestAngleType")), RestAngleType,{
					FName(TEXT("XPBDRestAngleType")),
					FName(TEXT("RestAngleType"))  
				}, ECollectionPropertyFlags::None);
			
			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoFlatnessRatio")), FlatnessRatio, {
				FName(TEXT("XPBDFlatnessRatio")),
				FName(TEXT("FlatnessRatio"))       
			});
			
			PropertyHelper.SetPropertyWeighted(FName(TEXT("XPBDAnisoRestAngle")), RestAngle, {
				FName(TEXT("XPBDRestAngle")),
				FName(TEXT("RestAngle"))       
			});
		}
	}
	else
	{
		if(ConstraintType == EChaosClothAssetBendingConstraintType::FacesSpring)
		{
			PropertyHelper.SetPropertyWeighted(FName(TEXT("BendingSpringStiffness")), BendingStiffness, {
				FName(TEXT("BendingElementStiffness")),
				FName(TEXT("XPBDBendingSpringStiffness")),    
				FName(TEXT("XPBDBendingElementStiffness")),   
				FName(TEXT("XPBDAnisoBendingStiffnessWarp"))});
		}
		else
		{
			PropertyHelper.SetPropertyWeighted(FName(TEXT("BendingElementStiffness")), BendingStiffness, {
				FName(TEXT("BendingSpringStiffness")),        // Existing properties to warn against
				FName(TEXT("XPBDBendingSpringStiffness")),
				FName(TEXT("XPBDBendingElementStiffness")),   
				FName(TEXT("XPBDAnisoBendingStiffnessWarp"))});
			
			PropertyHelper.SetPropertyWeighted(FName(TEXT("BucklingStiffness")), BucklingStiffness, {
				FName(TEXT("XPBDBucklingStiffness")),
				FName(TEXT("XPBDAnisoBucklingStiffnessWarp"))});
			
			PropertyHelper.SetProperty(FName(TEXT("BucklingRatios")), BucklingRatio, {
				FName(TEXT("XPBDBucklingRatio")),
				FName(TEXT("XPBDAnisoBucklingRatio"))});
		}
		
		PropertyHelper.SetPropertyEnum(FName(TEXT("RestAngleType")), RestAngleType, {
			FName(TEXT("XPBDAnisoRestAngleType")),
			FName(TEXT("XPBDRestAngleType")) 
		}, ECollectionPropertyFlags::None);
		
		PropertyHelper.SetPropertyWeighted(FName(TEXT("FlatnessRatio")), FlatnessRatio, {
				FName(TEXT("XPBDAnisoFlatnessRatio")),
				FName(TEXT("XPBDFlatnessRatio"))       
			});
		PropertyHelper.SetPropertyWeighted(FName(TEXT("RestAngle")), RestAngle, {
			FName(TEXT("XPBDAnisoRestAngle")), 
			FName(TEXT("XPBDRestAngle"))       
		});
	}
}
