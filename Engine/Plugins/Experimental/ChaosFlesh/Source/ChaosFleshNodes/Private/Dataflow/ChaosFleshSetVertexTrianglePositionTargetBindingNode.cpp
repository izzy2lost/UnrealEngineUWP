// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshSetVertexTrianglePositionTargetBindingNode.h"

#include "Chaos/BoundingVolumeHierarchy.h"
#include "GeometryCollection/Facades/CollectionMeshFacade.h"
#include "GeometryCollection/Facades/CollectionPositionTargetFacade.h"
#include "ChaosFlesh/TetrahedralCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosFleshSetVertexTrianglePositionTargetBindingNode)

//DEFINE_LOG_CATEGORY_STATIC(ChaosFleshSetVertexTrianglePositionTargetBindingNodeLog, Log, All);

void FSetVertexTrianglePositionTargetBindingDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		DataType InCollection = GetValue<DataType>(Context, &Collection);

		if (TManagedArray<FVector3f>* Vertices = InCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup))
			{
				if (TManagedArray<FIntVector>* Indices = InCollection.FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup))
				{
					GeometryCollection::Facades::FCollectionMeshFacade MeshFacade(InCollection);
					TArray<int32> ComponentIndex = MeshFacade.GetGeometryGroupIndexArray();
					TArray<Chaos::TVector<int32, 3>> IndicesArray;
					for (int32 i = 0; i < Indices->Num(); i++)
					{
						Chaos::TVector<int32, 3> CurrentIndices(0);
						for (int32 j = 0; j < 3; j++) 
						{
							CurrentIndices[j] = (*Indices)[i][j];
						}
						if (CurrentIndices[0] != -1
							&& CurrentIndices[1] != -1
							&& CurrentIndices[2] != -1)
						{
							IndicesArray.Emplace(CurrentIndices);
						}
					}
					TArray<TArray<int32>> LocalIndex;
					TArray<TArray<int32>>* LocalIndexPtr = &LocalIndex;
					TArray<TArray<int>> GlobalIndex = Chaos::Utilities::ComputeIncidentElements(IndicesArray, LocalIndexPtr);
					int32 ActualParticleCount = 0;
					for (int32 l = 0; l < GlobalIndex.Num(); l++)
					{
						if (GlobalIndex[l].Num() > 0)
						{
							ActualParticleCount += 1;
						}
					}
					TArray<Chaos::TVector<float, 3>> IndicesPositions; 
					IndicesPositions.SetNum(ActualParticleCount);
					TArray<int32> IndicesMap;
					IndicesMap.SetNum(ActualParticleCount);
					int32 CurrentParticleIndex = 0;
					for (int32 i = 0; i < GlobalIndex.Num(); i++)
					{
						if (GlobalIndex[i].Num() > 0)
						{
							IndicesPositions[CurrentParticleIndex] = (*Vertices)[(*Indices)[GlobalIndex[i][0]][LocalIndex[i][0]]];
							IndicesMap[CurrentParticleIndex] = (*Indices)[GlobalIndex[i][0]][LocalIndex[i][0]];
							CurrentParticleIndex += 1;
						}
					}
					Chaos::FReal SphereRadius = (Chaos::FReal)0.;

					Chaos::TVec3<float> CoordMaxs(-FLT_MAX);
					Chaos::TVec3<float> CoordMins(FLT_MAX);
					for (int32 i = 0; i < IndicesPositions.Num(); i++)
					{
						for (int32 j = 0; j < 3; j++)
						{
							if (IndicesPositions[i][j] > CoordMaxs[j])
							{
								CoordMaxs[j] = IndicesPositions[i][j];
							}
							if (IndicesPositions[i][j] < CoordMins[j])
							{
								CoordMins[j] = IndicesPositions[i][j];
							}
						}
					}
					Chaos::TVec3<float> CoordDiff = (CoordMaxs - CoordMins) * VertexRadiusRatio;
					SphereRadius = Chaos::FReal(FGenericPlatformMath::Min(CoordDiff[0], FGenericPlatformMath::Min(CoordDiff[1], CoordDiff[2])));

					TArray<Chaos::TSphere<Chaos::FReal, 3>*> VertexSpherePtrs;
					TArray<Chaos::TSphere<Chaos::FReal, 3>> VertexSpheres;

					VertexSpheres.Init(Chaos::TSphere<Chaos::FReal, 3>(Chaos::TVec3<Chaos::FReal>(0), SphereRadius), IndicesPositions.Num());
					VertexSpherePtrs.SetNum(IndicesPositions.Num());

					for (int32 i = 0; i < IndicesPositions.Num(); i++)
					{
						Chaos::TVec3<Chaos::FReal> SphereCenter(IndicesPositions[i]);
						Chaos::TSphere<Chaos::FReal, 3> VertexSphere(SphereCenter, SphereRadius);
						VertexSpheres[i] = Chaos::TSphere<Chaos::FReal, 3>(SphereCenter, SphereRadius);
						VertexSpherePtrs[i] = &VertexSpheres[i];
					}
					Chaos::TBoundingVolumeHierarchy<
						TArray<Chaos::TSphere<Chaos::FReal, 3>*>,
						TArray<int32>,
						Chaos::FReal,
						3> VertexBVH(VertexSpherePtrs);

					GeometryCollection::Facades::FPositionTargetFacade PositionTargets(InCollection);
					PositionTargets.DefineSchema();

					for (int32 i = 0; i < Indices->Num(); i++)
					{
						TArray<int32> TriangleIntersections0 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][0]]);
						TArray<int32> TriangleIntersections1 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][1]]);
						TArray<int32> TriangleIntersections2 = VertexBVH.FindAllIntersections((*Vertices)[(*Indices)[i][2]]);
						TriangleIntersections0.Sort();
						TriangleIntersections1.Sort();
						TriangleIntersections2.Sort();

						TArray<int32> TriangleIntersections({});
						for (int32 k = 0; k < TriangleIntersections0.Num(); k++)
						{
							if (TriangleIntersections1.Contains(TriangleIntersections0[k]) 
								&& TriangleIntersections2.Contains(TriangleIntersections0[k]))
							{
								TriangleIntersections.Emplace(TriangleIntersections0[k]);
							}
						}

						int32 TriangleIndex = ComponentIndex[(*Indices)[i][0]];
						int32 MinIndex = -1;
						float MinDis = SphereRadius;
						Chaos::TVector<float, 3> ClosestBary(0.f);
						for (int32 j = 0; j < TriangleIntersections.Num(); j++)
						{
							if (ComponentIndex[IndicesMap[TriangleIntersections[j]]] >= 0 && TriangleIndex >= 0 && ComponentIndex[IndicesMap[TriangleIntersections[j]]] != TriangleIndex)
							{
								Chaos::TVector<float, 3> Bary, TriPos0((*Vertices)[(*Indices)[i][0]]), TriPos1((*Vertices)[(*Indices)[i][1]]), TriPos2((*Vertices)[(*Indices)[i][2]]), ParticlePos((*Vertices)[IndicesMap[TriangleIntersections[j]]]);
								Chaos::TVector<Chaos::FRealSingle, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
								Chaos::FRealSingle CurrentDistance = ((*Vertices)[IndicesMap[TriangleIntersections[j]]] - ClosestPoint).Size();
								if (CurrentDistance < MinDis)
								{
									MinDis = CurrentDistance;
									MinIndex = IndicesMap[TriangleIntersections[j]];
									ClosestBary = Bary;
								}

							}
						}
						if (MinIndex != -1
							&& MinIndex != (*Indices)[i][0]
							&& MinIndex != (*Indices)[i][1]
							&& MinIndex != (*Indices)[i][2])
						{
							GeometryCollection::Facades::FPositionTargetsData DataPackage;
							DataPackage.TargetIndex.Init(MinIndex, 1);
							DataPackage.TargetWeights.Init(1.f, 1);
							DataPackage.SourceWeights.Init(1.f, 3);
							DataPackage.SourceIndex.Init(-1, 3);
							DataPackage.SourceIndex[0] = (*Indices)[i][0];
							DataPackage.SourceIndex[1] = (*Indices)[i][1];
							DataPackage.SourceIndex[2] = (*Indices)[i][2];
							DataPackage.SourceWeights[0] = ClosestBary[0];
							DataPackage.SourceWeights[1] = ClosestBary[1];
							DataPackage.SourceWeights[2] = ClosestBary[2];
							if (TManagedArray<float>* Mass = InCollection.FindAttribute<float>("Mass", FGeometryCollection::VerticesGroup))
							{
								DataPackage.Stiffness = 0.f;
								for (int32 k = 0; k < 3; k++)
								{
									DataPackage.Stiffness += DataPackage.SourceWeights[k] * PositionTargetStiffness * (*Mass)[DataPackage.SourceIndex[k]];
								}
								DataPackage.Stiffness += DataPackage.TargetWeights[0] * PositionTargetStiffness * (*Mass)[DataPackage.TargetIndex[0]];
								DataPackage.Stiffness /= 2.f;
							}
							else
							{
								DataPackage.Stiffness = PositionTargetStiffness;
							}
							PositionTargets.AddPositionTarget(DataPackage);
						}
					}
				}
		}
		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}
