// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshComputeMuscleActivationNode.h"

#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/Facades/CollectionMuscleActivationFacade.h"
#include "ChaosFlesh/TetrahedralCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshComputeMuscleActivationNode)

void FComputeMuscleActivationDataNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		TArray<int32> InOriginIndices = GetValue<TArray<int32>>(Context, &OriginIndicesIn);
		TArray<int32> InInsertionIndices = GetValue<TArray<int32>>(Context, &InsertionIndicesIn);
		TArray<float> OriginInsertionRestLength;
		if (InOriginIndices.Num() > 0 && InInsertionIndices.Num() > 0)
		{
			if (TManagedArray<FVector3f>* Vertex = InCollection.FindAttribute<FVector3f>("Vertex", "Vertices"))
			{
				if (TManagedArray<FIntVector4>* Elements = InCollection.FindAttribute<FIntVector4>(FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup))
				{
					if (TManagedArray<FVector3f>* FiberDirections = InCollection.FindAttribute<FVector3f>("FiberDirection", FTetrahedralCollection::TetrahedralGroup))
					{
						GeometryCollection::Facades::FMuscleActivationFacade FMuscleActivation(InCollection);
						TArray<TArray<int32>> MuscleActivationElements;
						TArray<int32> OriginNodes;
						TArray<int32> InsertionNodes;
						GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);
						TArray<int32> ComponentIndex = MeshFacade.GetGeometryGroupIndexArray();
						TMap<int32, int32> ComponentToIndex; //Component index to muscle index
						for (int32 i = 0; i < InOriginIndices.Num(); i++)
						{
							if (!ComponentToIndex.Contains(ComponentIndex[InOriginIndices[i]]))
							{
								//TODO: use one pair of separately defined origin and insertion for each muscle instead of choosing the first from kinematic origins and last from insertions
								ComponentToIndex.Add(ComponentIndex[InOriginIndices[i]], OriginNodes.Num());
								OriginNodes.Add(InOriginIndices[i]);
							}
						}
						InsertionNodes.Init(INDEX_NONE, OriginNodes.Num());
						for (int32 i = 0; i < InInsertionIndices.Num(); i++)
						{
							if (!ComponentToIndex.Contains(ComponentIndex[InInsertionIndices[i]]))
							{
								ensureMsgf(false, TEXT("No origin in this component"));
							}
							else if (InsertionNodes[ComponentToIndex[ComponentIndex[InInsertionIndices[i]]]] == INDEX_NONE)
							{
								InsertionNodes[ComponentToIndex[ComponentIndex[InInsertionIndices[i]]]] = InInsertionIndices[i];
							}
						}
						MuscleActivationElements.SetNum(OriginNodes.Num());
						for (int32 ElemIdx = 0; ElemIdx < Elements->Num(); ElemIdx++)
						{
							if (ComponentToIndex.Contains(ComponentIndex[(*Elements)[ElemIdx][0]]))
							{
								MuscleActivationElements[ComponentToIndex[ComponentIndex[(*Elements)[ElemIdx][0]]]].Add(ElemIdx);
							}
						}
						OriginInsertionRestLength.SetNum(OriginNodes.Num());

						for (int32 MuscleComponentIdx = 0; MuscleComponentIdx < OriginNodes.Num(); MuscleComponentIdx++)
						{
							if (ensureMsgf(InsertionNodes[MuscleComponentIdx] != INDEX_NONE, TEXT("InsertionNodes[%d] is not a pair"), MuscleComponentIdx))
							{
								GeometryCollection::Facades::FMuscleActivationData MuscleActivationData;
								MuscleActivationData.MuscleActivationElement = MuscleActivationElements[MuscleComponentIdx];
								MuscleActivationData.OriginInsertionPair = FIntVector2(OriginNodes[MuscleComponentIdx], InsertionNodes[MuscleComponentIdx]);
								MuscleActivationData.OriginInsertionRestLength = ((*Vertex)[OriginNodes[MuscleComponentIdx]] - (*Vertex)[InsertionNodes[MuscleComponentIdx]]).Size();
								MuscleActivationData.FiberDirectionMatrix.SetNum(MuscleActivationElements[MuscleComponentIdx].Num());
								for (int32 LocalElemIdx = 0; LocalElemIdx < MuscleActivationElements[MuscleComponentIdx].Num(); LocalElemIdx++)
								{
									FVector3f V = (*FiberDirections)[MuscleActivationElements[MuscleComponentIdx][LocalElemIdx]];
									// QR decomposition on vvT
									FVector3f W = V;
									if (V.X < V.Y)
									{
										W.X += 1.f;
									}
									else
									{
										W.Y += 1.f;
									}
									FVector3f U = (V ^ W).GetSafeNormal();
									W = (U ^ V).GetSafeNormal();
									MuscleActivationData.FiberDirectionMatrix[LocalElemIdx] = Chaos::PMatrix33d(V, W, U);
								}
								FMuscleActivation.AddMuscleActivationData(MuscleActivationData);
							}
						}
					}
				}
			}
		}
		Out->SetValue(MoveTemp(InCollection), Context);
	}
}
