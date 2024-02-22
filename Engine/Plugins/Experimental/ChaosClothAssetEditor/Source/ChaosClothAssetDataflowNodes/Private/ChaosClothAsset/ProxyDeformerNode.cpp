// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ProxyDeformerNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/ClothCollectionAttribute.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Utils/ClothingMeshUtils.h"
#include "PointWeightMap.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProxyDeformerNode)

#define LOCTEXT_NAMESPACE "ChaosClothAssetProxyDeformerNode"

namespace UE::Chaos::ClothAsset::Private
{
	struct FDeformerMappingDataGenerator
	{
		TConstArrayView<FVector3f> SimPositions;
		TConstArrayView<FIntVector3> SimIndices;
		TConstArrayView<float> MaxDistances;
		TConstArrayView<FVector3f> RenderPositions;
		TConstArrayView<FVector3f> RenderNormals;
		TConstArrayView<FIntVector3> RenderIndices;
		TArrayView<TArray<FVector4f>> RenderDeformerPositionBaryCoordsAndDist;
		TArrayView<TArray<FVector4f>> RenderDeformerNormalBaryCoordsAndDist;
		TArrayView<TArray<FVector4f>> RenderDeformerTangentBaryCoordsAndDist;
		TArrayView<TArray<FIntVector3>> RenderDeformerSimIndices3D;
		TArrayView<TArray<float>> RenderDeformerWeight;
		TArrayView<float> RenderDeformerSkinningBlend;

		int32 Generate(bool bUseSmoothTransition, bool bUseMultipleInfluences, float InfluenceRadius)
		{
			check(RenderPositions.Num() == RenderNormals.Num());
			check(RenderPositions.Num() == RenderDeformerPositionBaryCoordsAndDist.Num());
			check(RenderPositions.Num() == RenderDeformerNormalBaryCoordsAndDist.Num());
			check(RenderPositions.Num() == RenderDeformerTangentBaryCoordsAndDist.Num());
			check(RenderPositions.Num() == RenderDeformerSimIndices3D.Num());
			check(RenderPositions.Num() == RenderDeformerWeight.Num());
			check(RenderPositions.Num() == RenderDeformerSkinningBlend.Num());

			if (!ensureMsgf(SimPositions.Num() <= (int32)TNumericLimits<uint16>::Max() + 1, TEXT("FMeshToMeshVertData data is limited to 16bit unsigned int indexes (65536 indices max).")))
			{
				return 0;
			}

			TArray<uint32> ScalarSimIndices;
			ScalarSimIndices.Reserve(SimIndices.Num() * 3);
			for (const FIntVector3& SimIndex : SimIndices)
			{
				ScalarSimIndices.Add(SimIndex[0]);
				ScalarSimIndices.Add(SimIndex[1]);
				ScalarSimIndices.Add(SimIndex[2]);
			}
			TArray<uint32> ScalarRenderIndices;
			ScalarRenderIndices.Reserve(RenderIndices.Num() * 3);
			for (const FIntVector3& RenderIndex : RenderIndices)
			{
				ScalarRenderIndices.Add(RenderIndex[0]);
				ScalarRenderIndices.Add(RenderIndex[1]);
				ScalarRenderIndices.Add(RenderIndex[2]);
			}

			const ClothingMeshUtils::ClothMeshDesc SimMeshDesc(SimPositions, ScalarSimIndices);
			const ClothingMeshUtils::ClothMeshDesc RenderMeshDesc(RenderPositions, RenderNormals, ScalarRenderIndices);

			FPointWeightMap PointWeightMap = (MaxDistances.Num() == SimPositions.Num()) ? FPointWeightMap(MaxDistances) : FPointWeightMap(SimPositions.Num(), 1.f);

			TArray<FMeshToMeshVertData> MeshToMeshVertData;

			ClothingMeshUtils::GenerateMeshToMeshVertData(
				MeshToMeshVertData,
				RenderMeshDesc,
				SimMeshDesc,
				&PointWeightMap,
				bUseSmoothTransition,
				bUseMultipleInfluences,
				InfluenceRadius);

			const int32 NumInfluences = MeshToMeshVertData.Num() / RenderPositions.Num();
			check(MeshToMeshVertData.Num() == RenderPositions.Num() * NumInfluences);  // Check modulo
			check((!bUseMultipleInfluences && NumInfluences == 1) || (bUseMultipleInfluences && NumInfluences > 1));

			for (int32 Index = 0; Index < RenderPositions.Num(); ++Index)
			{
				RenderDeformerPositionBaryCoordsAndDist[Index].SetNum(NumInfluences);
				RenderDeformerNormalBaryCoordsAndDist[Index].SetNum(NumInfluences);
				RenderDeformerTangentBaryCoordsAndDist[Index].SetNum(NumInfluences);
				RenderDeformerSimIndices3D[Index].SetNum(NumInfluences);
				RenderDeformerWeight[Index].SetNum(NumInfluences);

				RenderDeformerSkinningBlend[Index] = 0.f;

				for (int32 Influence = 0; Influence < NumInfluences; ++Influence)
				{
					const FMeshToMeshVertData& MeshToMeshVertDatum = MeshToMeshVertData[Index * NumInfluences + Influence];

					RenderDeformerPositionBaryCoordsAndDist[Index][Influence] = MeshToMeshVertDatum.PositionBaryCoordsAndDist;
					RenderDeformerNormalBaryCoordsAndDist[Index][Influence] = MeshToMeshVertDatum.NormalBaryCoordsAndDist;
					RenderDeformerTangentBaryCoordsAndDist[Index][Influence] = MeshToMeshVertDatum.TangentBaryCoordsAndDist;
					RenderDeformerSimIndices3D[Index][Influence] = FIntVector3(
						MeshToMeshVertDatum.SourceMeshVertIndices[0],
						MeshToMeshVertDatum.SourceMeshVertIndices[1],
						MeshToMeshVertDatum.SourceMeshVertIndices[2]);
					RenderDeformerWeight[Index][Influence] = MeshToMeshVertDatum.Weight;

					RenderDeformerSkinningBlend[Index] += MeshToMeshVertDatum.Weight * (float)MeshToMeshVertDatum.SourceMeshVertIndices[3] / (float)TNumericLimits<uint16>::Max();
				}
			}
			return NumInfluences;
		}
	};
}

FChaosClothAssetProxyDeformerNode::FChaosClothAssetProxyDeformerNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid)
	: FDataflowNode(InParam, InGuid)
{
	using namespace UE::Chaos::ClothAsset;
	MaxDistance.WeightMap = FString();  // An empty weight map is an accepted input, but a non existing one isn't
	SkinningBlendName = ClothCollectionAttribute::RenderDeformerSkinningBlend.ToString();

	RegisterInputConnection(&Collection);
	RegisterInputConnection(&MaxDistance.WeightMap, GET_MEMBER_NAME_CHECKED(FChaosClothAssetWeightedValueNonAnimatableNoLowHighRange, WeightMap));
	RegisterOutputConnection(&Collection, &Collection);
	RegisterOutputConnection(&SkinningBlendName);
}

void FChaosClothAssetProxyDeformerNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		using namespace UE::Chaos::ClothAsset;

		// Update the weight map override
		MaxDistance.WeightMap_Override = GetValue<FString>(Context, &MaxDistance.WeightMap, UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden);

		// Evaluate in collection
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		const TSharedRef<FManagedArrayCollection> ClothCollection = MakeShared<FManagedArrayCollection>(MoveTemp(InCollection));

		// Always check for a valid cloth collection/facade to avoid processing non cloth collections
		FCollectionClothFacade ClothFacade(ClothCollection);
		if (ClothFacade.IsValid() && ClothFacade.HasValidData())
		{
			// Retrieve the MaxDistance weight map name
			FName MaxDistanceWeightMapName = FName(*GetValue<FString>(Context, &MaxDistance.WeightMap));
			if (MaxDistanceWeightMapName != NAME_None && !ClothFacade.HasWeightMap(MaxDistanceWeightMapName))
			{
				FClothDataflowTools::LogAndToastWarning(*this,
					LOCTEXT("ClothFacadeHasWeightMapHeadline", "Unknown MaxDistance weight map."),
					LOCTEXT("ClothFacadeHasWeightMapDetails", "The specified MaxDistance weight map does't exist within the input Cloth Collection."));

				MaxDistanceWeightMapName = NAME_None;
			}

			// Add the optional render deformer schema
			if (!ClothFacade.IsValid(EClothCollectionOptionalSchemas::RenderDeformer))
			{
				ClothFacade.DefineSchema(EClothCollectionOptionalSchemas::RenderDeformer);
			}

			// Create the render weight map for storing the skinning blend weights
			Private::FDeformerMappingDataGenerator DeformerMappingDataGenerator;
			DeformerMappingDataGenerator.SimPositions = ClothFacade.GetSimPosition3D();
			DeformerMappingDataGenerator.SimIndices = ClothFacade.GetSimIndices3D();
			DeformerMappingDataGenerator.MaxDistances = ClothFacade.GetWeightMap(MaxDistanceWeightMapName);
			DeformerMappingDataGenerator.RenderPositions = ClothFacade.GetRenderPosition();
			DeformerMappingDataGenerator.RenderNormals = ClothFacade.GetRenderNormal();
			DeformerMappingDataGenerator.RenderIndices = ClothFacade.GetRenderIndices();
			DeformerMappingDataGenerator.RenderDeformerPositionBaryCoordsAndDist = ClothFacade.GetRenderDeformerPositionBaryCoordsAndDist();
			DeformerMappingDataGenerator.RenderDeformerNormalBaryCoordsAndDist = ClothFacade.GetRenderDeformerNormalBaryCoordsAndDist();
			DeformerMappingDataGenerator.RenderDeformerTangentBaryCoordsAndDist = ClothFacade.GetRenderDeformerTangentBaryCoordsAndDist();
			DeformerMappingDataGenerator.RenderDeformerSimIndices3D = ClothFacade.GetRenderDeformerSimIndices3D();
			DeformerMappingDataGenerator.RenderDeformerWeight = ClothFacade.GetRenderDeformerWeight();
			DeformerMappingDataGenerator.RenderDeformerSkinningBlend = ClothFacade.GetRenderDeformerSkinningBlend();

			const int32 NumInfluences = DeformerMappingDataGenerator.Generate(bUseSmoothTransition, bUseMultipleInfluences, InfluenceRadius);

			for (int32 RenderPatternIndex = 0; RenderPatternIndex < ClothFacade.GetNumRenderPatterns(); ++RenderPatternIndex)
			{
				FCollectionClothRenderPatternFacade RenderPatternFacade = ClothFacade.GetRenderPattern(RenderPatternIndex);
				RenderPatternFacade.SetRenderDeformerNumInfluences(NumInfluences);
			}
		}

		SetValue(Context, MoveTemp(*ClothCollection), &Collection);
	}
}

#undef LOCTEXT_NAMESPACE
