// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SimulationFabricConfigNode.h"
#include "ClothConfig.h"
#include "ChaosClothAsset/SimulationBaseConfigNodePrivate.h"
#include "ChaosClothAsset/CollectionClothFacade.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SimulationFabricConfigNode)

namespace UE::Chaos::ClothAsset::Private
{
	static FWeightedValueBounds ComputeWeightedValueBounds(FCollectionClothFacade& ClothFacade,
		 TArray<float>& PatternValues, const TFunction<float(const FCollectionClothFabricFacade&)>& FabricValueFunction)
	{
		const int32 NumPatterns = ClothFacade.GetNumSimPatterns();
		
		PatternValues.Init(0.0f, NumPatterns);
		float MinValue = FLT_MAX, MaxValue = 0.0f;
		
		for(int32 PatternIndex = 0; PatternIndex < NumPatterns; ++PatternIndex)
		{
			FCollectionClothSimPatternFacade PatternFacade = ClothFacade.GetSimPattern(PatternIndex);
			const int32 FabricIndex = PatternFacade.GetFabricIndex();

			if(FabricIndex >= 0 && FabricIndex < ClothFacade.GetNumFabrics())
			{
				FCollectionClothFabricFacade FabricFacade = ClothFacade.GetFabric(FabricIndex);
				const float FabricValue = FabricValueFunction(FabricFacade);

				MinValue = FMath::Min(MinValue, FabricValue);
				MaxValue = FMath::Max(MaxValue, FabricValue);

				PatternValues[PatternIndex] = FabricValue;
			}
		}
		return {MinValue, MaxValue};
	}

	static FWeightedValueBounds BuildFabricWeightedValue(FCollectionClothFacade& ClothFacade,
					  const FString& WeightMapName, const TFunction<float(const FCollectionClothFabricFacade&)>& FabricValueFunction)
	{
		const int32 NumPatterns = ClothFacade.GetNumSimPatterns();

		TArray<float> PatternValues;
		const FWeightedValueBounds WeightValueBounds = ComputeWeightedValueBounds(ClothFacade, PatternValues, FabricValueFunction);

		
		if(WeightValueBounds.Low != WeightValueBounds.High)
		{
			const int32 NumVertices = ClothFacade.GetNumSimVertices3D();
		
			TArray<int32> NumValues;
			NumValues.Init(0, NumVertices);

			TArray<float> VertexValues;
			VertexValues.Init(0.0f, NumVertices);
			
			ClothFacade.AddWeightMap(*WeightMapName);
			const TArrayView<float> ValueWeightMap = ClothFacade.GetWeightMap(*WeightMapName);
			
			for(int32 PatternIndex = 0; PatternIndex < NumPatterns; ++PatternIndex)
			{
				FCollectionClothSimPatternFacade PatternFacade = ClothFacade.GetSimPattern(PatternIndex);
				
				const TConstArrayView<int32> SimVertex3DLookup =
					static_cast<FCollectionClothSimPatternConstFacade&>(PatternFacade).GetSimVertex3DLookup();

				// Average of the pattern values at the seam
				for(const int32& SimVertex3DIndex : SimVertex3DLookup)
				{
					VertexValues[SimVertex3DIndex] += PatternValues[PatternIndex];
					NumValues[SimVertex3DIndex]++;
				}
			}
			for(int32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
            {
				if(NumValues[VertexIndex] > 0)
				{
					ValueWeightMap[VertexIndex] = (VertexValues[VertexIndex] / NumValues[VertexIndex]  - WeightValueBounds.Low) /
						(WeightValueBounds.High - WeightValueBounds.Low);
				}
            }
		}
		
		return WeightValueBounds;
	}

	static FWeightedValueBounds BuildFabricValue(FCollectionClothFacade& ClothFacade,
					  const TFunction<float(const FCollectionClothFabricFacade&)>& FabricValueFunction)
	{
		TArray<float> PatternValues;
		return ComputeWeightedValueBounds(ClothFacade, PatternValues, FabricValueFunction);
	}
}

FChaosClothAssetSimulationFabricConfigNode::FChaosClothAssetSimulationFabricConfigNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FChaosClothAssetSimulationBaseConfigNode(InParam, InGuid)
{}

void FChaosClothAssetSimulationFabricConfigNode::SetFabricProperty(const FName& PropertyName, UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade,
	FPropertyHelper& PropertyHelper, const TFunction<float(const UE::Chaos::ClothAsset::FCollectionClothFabricFacade&)>& FabricValueFunction,
	const TArray<FName>& SimilarPropertyNames, ECollectionPropertyFlags PropertyFlags)
{
	const UE::Chaos::ClothAsset::FWeightedValueBounds WeightedValueBounds =
			UE::Chaos::ClothAsset::Private::BuildFabricValue(ClothFacade, FabricValueFunction);

	const float AveragedValue = 0.5f * (WeightedValueBounds.Low + WeightedValueBounds.High);
	PropertyHelper.SetProperty(PropertyName, AveragedValue, SimilarPropertyNames, PropertyFlags);
}

template<typename PropertyType>
void FChaosClothAssetSimulationFabricConfigNode::SetFabricPropertyWeighted(
	const FName& PropertyName, const PropertyType& PropertyValue,  UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade,
	FPropertyHelper& PropertyHelper, const TFunction<float(const UE::Chaos::ClothAsset::FCollectionClothFabricFacade&)>& FabricValueFunction,
	const TArray<FName>& SimilarPropertyNames, ECollectionPropertyFlags PropertyFlags)
{
	if(PropertyValue.bCouldUseFabrics && (PropertyValue.bImportFabricBounds || PropertyValue.bBuildFabricMaps))
	{
		UE::Chaos::ClothAsset::FWeightedValueBounds WeightedValueBounds;
		if(PropertyValue.bBuildFabricMaps)
		{
			WeightedValueBounds = UE::Chaos::ClothAsset::Private::BuildFabricWeightedValue(ClothFacade, 
			 PropertyHelper.GetPropertyString(&PropertyValue.WeightMap), FabricValueFunction);
		}
		else 
		{
			WeightedValueBounds = UE::Chaos::ClothAsset::Private::BuildFabricValue(
				ClothFacade, FabricValueFunction);
		}
		if(PropertyValue.bImportFabricBounds)
		{
			PropertyValue.Low = WeightedValueBounds.Low;
			PropertyValue.High = WeightedValueBounds.High;
		}
	}
	PropertyHelper.SetPropertyWeighted(PropertyName, PropertyValue, SimilarPropertyNames, PropertyFlags);
}

template void FChaosClothAssetSimulationFabricConfigNode::SetFabricPropertyWeighted<FChaosClothAssetWeightedValue>(const FName& PropertyName,
	const FChaosClothAssetWeightedValue& PropertyValue,  UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade,
	FPropertyHelper& PropertyHelper, const TFunction<float(const UE::Chaos::ClothAsset::FCollectionClothFabricFacade&)>& FabricValueFunction,
	const TArray<FName>& SimilarPropertyNames, ECollectionPropertyFlags PropertyFlags);

template void FChaosClothAssetSimulationFabricConfigNode::SetFabricPropertyWeighted<FChaosClothAssetWeightedValueNonAnimatable>(const FName& PropertyName,
	const FChaosClothAssetWeightedValueNonAnimatable& PropertyValue,  UE::Chaos::ClothAsset::FCollectionClothFacade& ClothFacade,
	FPropertyHelper& PropertyHelper, const TFunction<float(const UE::Chaos::ClothAsset::FCollectionClothFabricFacade&)>& FabricValueFunction,
	const TArray<FName>& SimilarPropertyNames, ECollectionPropertyFlags PropertyFlags);
