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
		TManagedArray<float>* ContractionVolumeScalePerVertex = InCollection.FindAttribute<float>("ContractionVolumeScale", "Vertices");
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
						TArray<TArray<int32>> ComponentOrigins; //One origin node per muscle component
						TArray<TArray<int32>> ComponentInsertions; //One insertion node per muscle component
						GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);
						TArray<int32> ComponentIndex = MeshFacade.GetGeometryGroupIndexArray();
						TMap<int32, int32> ComponentToMuscleIndex; //Component index to muscle index
						for (int32 i = 0; i < InOriginIndices.Num(); i++)
						{
							if (!ComponentToMuscleIndex.Contains(ComponentIndex[InOriginIndices[i]]))
							{
								ComponentToMuscleIndex.Add(ComponentIndex[InOriginIndices[i]], ComponentOrigins.Num());
								ComponentOrigins.SetNum(ComponentOrigins.Num() + 1);
								ComponentOrigins[ComponentOrigins.Num()-1].Add(InOriginIndices[i]);
							}
							else
							{
								ComponentOrigins[ComponentToMuscleIndex[ComponentIndex[InOriginIndices[i]]]].Add(InOriginIndices[i]);
							}
						}
						ComponentInsertions.SetNum(ComponentOrigins.Num());
						for (int32 i = 0; i < InInsertionIndices.Num(); i++)
						{
							if (!ComponentToMuscleIndex.Contains(ComponentIndex[InInsertionIndices[i]]))
							{
								ensureMsgf(false, TEXT("No origin in this component"));
							}
							else
							{
								ComponentInsertions[ComponentToMuscleIndex[ComponentIndex[InInsertionIndices[i]]]].Add(InInsertionIndices[i]);
							}
						}
						MuscleActivationElements.SetNum(ComponentOrigins.Num());
						for (int32 ElemIdx = 0; ElemIdx < Elements->Num(); ElemIdx++)
						{
							if (ComponentToMuscleIndex.Contains(ComponentIndex[(*Elements)[ElemIdx][0]]))
							{
								MuscleActivationElements[ComponentToMuscleIndex[ComponentIndex[(*Elements)[ElemIdx][0]]]].Add(ElemIdx);
							}
						}
						//Choose one origin-insertion pair per muscle with largest distance apart
						//use origin-insertion line segment length to estimate activation
						for (int32 MuscleComponentIdx = 0; MuscleComponentIdx < ComponentOrigins.Num(); MuscleComponentIdx++)
						{
							if (ensureMsgf(ComponentOrigins.Num() > 0 && ComponentInsertions.Num() > 0, TEXT("Origin or Insertion missing in the muscle %d"), MuscleComponentIdx))
							{
								GeometryCollection::Facades::FMuscleActivationData MuscleActivationData;
								MuscleActivationData.OriginInsertionRestLength = 0;
								for (int32 OriginIdx : ComponentOrigins[MuscleComponentIdx])
								{
									for (int32 InsertionIdx : ComponentInsertions[MuscleComponentIdx])
									{
										float Dist = ((*Vertex)[OriginIdx] - (*Vertex)[InsertionIdx]).Size();
										if (Dist > MuscleActivationData.OriginInsertionRestLength)
										{
											MuscleActivationData.OriginInsertionPair = FIntVector2(OriginIdx, InsertionIdx);
											MuscleActivationData.OriginInsertionRestLength = Dist;
										}
									}
								}
								MuscleActivationData.MuscleActivationElement = MuscleActivationElements[MuscleComponentIdx];
								MuscleActivationData.FiberDirectionMatrix.SetNum(MuscleActivationElements[MuscleComponentIdx].Num());
								MuscleActivationData.ContractionVolumeScale.SetNum(MuscleActivationElements[MuscleComponentIdx].Num());
								for (int32 LocalElemIdx = 0; LocalElemIdx < MuscleActivationElements[MuscleComponentIdx].Num(); LocalElemIdx++)
								{
									FVector3f V = (*FiberDirections)[MuscleActivationElements[MuscleComponentIdx][LocalElemIdx]];
									// QR decomposition on vvT for orthogonal directions
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
									//Muscle contraction volume scale
									MuscleActivationData.ContractionVolumeScale[LocalElemIdx] = ContractionVolumeScale;
									if (ContractionVolumeScalePerVertex)
									{
										float AverageScale = 1.f;
										for (int32 ie = 0; ie < 4; ie++)
										{
											AverageScale += (*ContractionVolumeScalePerVertex)[(*Elements)[MuscleActivationElements[MuscleComponentIdx][LocalElemIdx]][ie]];
											AverageScale /= 4.f;
										}
										MuscleActivationData.ContractionVolumeScale[LocalElemIdx] *= AverageScale;
									}
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
