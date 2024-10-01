// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionTransferVertexAttributeNode.h"

#include "Chaos/Triangle.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/HierarchicalSpatialHash.h"
#include "Chaos/TriangleCollisionPoint.h"
#include "Dataflow/DataflowInputOutput.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionTransferVertexAttributeNode)
#define LOCTEXT_NAMESPACE "FGeometryCollectionTransferVertexAttributeNode"

namespace UE::Private {

	class FTransferFacade
	{
		const FManagedArrayCollection& ConstCollection;
		FManagedArrayCollection* Collection = nullptr;
	public:
		FTransferFacade(FManagedArrayCollection& InCollection)
			: ConstCollection(InCollection)
			, Collection(&InCollection)
			, BoneMap(InCollection, FName("BoneMap"), FName("Vertices"))
			, Vertex(InCollection, FName("Vertex"), FName("Vertices"))
			, Indices(InCollection, FName("Indices"), FName("Faces"))
			, Transform(InCollection, FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup)
			, Parent(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
			, VertexStart(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
			, VertexCount(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
			, FaceStart(InCollection, "FaceStart", FGeometryCollection::GeometryGroup)
			, FaceCount(InCollection, "FaceCount", FGeometryCollection::GeometryGroup)
		{}

		FTransferFacade(const FManagedArrayCollection& InCollection)
			: ConstCollection(InCollection)
			, Collection(nullptr)
			, BoneMap(InCollection, FName("BoneMap"), FName("Vertices"))
			, Vertex(InCollection, FName("Vertex"), FName("Vertices"))
			, Indices(InCollection, FName("Indices"), FName("Faces"))
			, Transform(InCollection, FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup)
			, Parent(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
			, VertexStart(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
			, VertexCount(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
			, FaceStart(InCollection, "FaceStart", FGeometryCollection::GeometryGroup)
			, FaceCount(InCollection, "FaceCount", FGeometryCollection::GeometryGroup)
		{}


		bool IsValid() const {
			return BoneMap.IsValid() && Vertex.IsValid() && Indices.IsValid() && Transform.IsValid() && Parent.IsValid() && VertexStart.IsValid() &&
				VertexCount.IsValid() && FaceStart.IsValid() && FaceCount.IsValid();
		}

		template<typename T>
		const TManagedArray<T>* GetAttributeArray(FString AttributeName, FString Group) const
		{
			return ConstCollection.FindAttributeTyped<T>(FName(AttributeName), FName(Group));
		}

		template<typename T>
		TManagedArray<T>* GetAttributeArray(FString AttributeName, FString Group)
		{
			TManagedArray<T>* TargetAttributeArray = nullptr;
			if (!Collection->HasAttribute(FName(AttributeName), FName(Group)))
			{
				Collection->AddAttribute<T>(FName(AttributeName), FName(Group));
			}
			return Collection->FindAttributeTyped<T>(FName(AttributeName), FName(Group));
		}


		TManagedArrayAccessor<int32> BoneMap;
		TManagedArrayAccessor<FVector3f> Vertex;
		TManagedArrayAccessor<FIntVector3> Indices;
		TManagedArrayAccessor<FTransform3f> Transform;
		TManagedArrayAccessor<int32> Parent;
		TManagedArrayAccessor<int32> VertexStart;
		TManagedArrayAccessor<int32> VertexCount;
		TManagedArrayAccessor<int32> FaceStart;
		TManagedArrayAccessor<int32> FaceCount;
	};

}

void FGeometryCollectionTransferVertexAttributeNode::Evaluate(UE::Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	FCollectionAttributeKey Key = GetValue(Context, &AttributeKey, AttributeKey);

	if (Out->IsA(&Collection))
	{
		FManagedArrayCollection TargetCollection = GetValue(Context, &Collection);
		const FManagedArrayCollection& SampleCollection = GetValue(Context, &FromCollection);

		UE::Private::FTransferFacade Target(TargetCollection);
		const UE::Private::FTransferFacade Sample(SampleCollection);

		if (Target.IsValid() && Sample.IsValid())
		{
			if (const TManagedArray<float>* SourceAttributeFloatArray = Sample.GetAttributeArray<float>(Key.Attribute, Key.Group))
			{
				TManagedArray<float>* TargetAttributeArray = Target.GetAttributeArray<float>(Key.Attribute, Key.Group);
				TargetAttributeArray->Fill(0.f);

				TArray<FIntVector2> AlignedGeometry = FindSourceToTargetGeometryMap(SampleCollection, TargetCollection);
				if (AlignedGeometry.Num() == TargetCollection.NumElements(FGeometryCollection::GeometryGroup))
				{
					PairedGeometryTransfer<float>(Key, AlignedGeometry, Sample, Target, SourceAttributeFloatArray, TargetAttributeArray);
				}
				else
				{
					NearestVertexTransfer<float>(Key, Sample, Target, SourceAttributeFloatArray, TargetAttributeArray);
				}
			}
			else if (const TManagedArray<FLinearColor>* SourceAttributeColorArray = Sample.GetAttributeArray<FLinearColor>(Key.Attribute, Key.Group))
			{
				TManagedArray<FLinearColor>* TargetAttributeArray = Target.GetAttributeArray<FLinearColor>(Key.Attribute, Key.Group);
				TargetAttributeArray->Fill(FLinearColor(0, 0, 0, 0));

				TArray<FIntVector2> AlignedGeometry = FindSourceToTargetGeometryMap(SampleCollection, TargetCollection);
				if (AlignedGeometry.Num() == TargetCollection.NumElements(FGeometryCollection::GeometryGroup))
				{
					PairedGeometryTransfer<FLinearColor>(Key, AlignedGeometry, Sample, Target, SourceAttributeColorArray, TargetAttributeArray);
				}
				else
				{
					NearestVertexTransfer<FLinearColor>(Key, Sample, Target, SourceAttributeColorArray, TargetAttributeArray);
				}
			}
		}

		SetValue(Context, MoveTemp(TargetCollection), &Collection);
	}
	else if (Out->IsA(&AttributeKey))
	{
		SetValue(Context, MoveTemp(Key), &AttributeKey);
	}
}


TArray<FIntVector2> FGeometryCollectionTransferVertexAttributeNode::FindSourceToTargetGeometryMap(const FManagedArrayCollection& AttributeCollectionVal, const FManagedArrayCollection& CollectionVal) const
{
	TArray<FIntVector2> Mapping;
	const TManagedArray<FString>* SourceName = AttributeCollectionVal.FindAttribute<FString>(FName("BoneName"), FTransformCollection::TransformGroup);
	const TManagedArray<int32>* SourceGeometryGroup = AttributeCollectionVal.FindAttribute<int32>(FName("TransformToGeometryIndex"), FTransformCollection::TransformGroup);
	const TManagedArray<FString>* TargetName = CollectionVal.FindAttribute<FString>(FName("BoneName"), FTransformCollection::TransformGroup);
	const TManagedArray<int32>* TargetGeometryGroup = CollectionVal.FindAttribute<int32>(FName("TransformToGeometryIndex"), FTransformCollection::TransformGroup);
	if (SourceName && SourceGeometryGroup && TargetName && TargetGeometryGroup)
	{
		for (int i = 0; i < SourceName->Num(); i++)
		{
			for (int j = 0; j < TargetName->Num(); j++)
			{
				FString TestName = FString::Printf(TEXT("%s_Tet"), *(*SourceName)[i]);
				if ((*TargetName)[j].StartsWith(TestName))
				{
					Mapping.Add(FIntVector2((*SourceGeometryGroup)[i], (*TargetGeometryGroup)[j]));
					break;
				}
			}
		}
	}

	return Mapping;
}

template<typename T>
void FGeometryCollectionTransferVertexAttributeNode::PairedGeometryTransfer(FCollectionAttributeKey Key, const TArray<FIntVector2>& PairedGeometry,
	const UE::Private::FTransferFacade& Sample, UE::Private::FTransferFacade& Target, const TManagedArray<T>* SourceAttributeArray, TManagedArray<T>* TargetAttributeArray) const
{
	//TargetAttributeArray has to be filled with empty values before (attribute types: float, FLinearColor)
	if (SourceAttributeArray && TargetAttributeArray)
	{
		Chaos::FReal SphereFullRadius;

		if (SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Asset_Edge || SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Asset_Bound)
		{
			// Build component space vertices for TargetCollection
			TArray<FVector3f> ComponentSpaceFullTargetVertices; // size of num vertices of the geometry entry. 
			BuildComponentSpaceVertices(Target.Transform.Get(), Target.Parent.Get(), Target.BoneMap.Get(), Target.Vertex.Get(),
				0, Target.Vertex.Num(), ComponentSpaceFullTargetVertices);

			// Build component space vertices for TargetCollection
			TArray<FVector3f> ComponentSpaceFullVertices; // size of num vertices of the geometry entry
			BuildComponentSpaceVertices(Sample.Transform.Get(), Sample.Parent.Get(), Sample.BoneMap.Get(), Sample.Vertex.Get(),
				0, Sample.Vertex.Num(), ComponentSpaceFullVertices);
			if (SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Asset_Edge)
			{
				SphereFullRadius = Chaos::FReal(EdgeMultiplier * FMath::Max(
					MaxEdgeLength(ComponentSpaceFullTargetVertices, Target.Indices.Get(), 0, 0, Target.Indices.Num()),
					MaxEdgeLength(ComponentSpaceFullVertices, Sample.Indices.Get(), 0, 0, Sample.Indices.Num())));
			}
			else if (SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Asset_Bound)
			{
				Chaos::TVec3<float> CoordMaxs(-FLT_MAX);
				Chaos::TVec3<float> CoordMins(FLT_MAX);
				for (int32 i = 0; i < ComponentSpaceFullVertices.Num(); i++)
				{
					for (int32 j = 0; j < 3; j++)
					{
						if (ComponentSpaceFullVertices[i][j] > CoordMaxs[j])
						{
							CoordMaxs[j] = ComponentSpaceFullVertices[i][j];
						}
						if (ComponentSpaceFullVertices[i][j] < CoordMins[j])
						{
							CoordMins[j] = ComponentSpaceFullVertices[i][j];
						}
					}
				}
				Chaos::TVec3<float> CoordDiff = (CoordMaxs - CoordMins) * BoundMultiplier;
				SphereFullRadius = Chaos::FReal(FGenericPlatformMath::Min(CoordDiff[0], FGenericPlatformMath::Min(CoordDiff[1], CoordDiff[2])));
			}
		}
		ParallelFor(PairedGeometry.Num(), [&](int32 Pdx)
			{
				int32 AttributeGeometryIndex = PairedGeometry[Pdx][0];
				int32 TargetGeometryIndex = PairedGeometry[Pdx][1];
				if (ensure(0 <= AttributeGeometryIndex && AttributeGeometryIndex < Sample.VertexStart.Num()))
				{
					if (ensure(0 <= TargetGeometryIndex && TargetGeometryIndex < Target.VertexStart.Num()))
					{
						// Build component space vertices for TargetCollection
						TArray<FVector3f> ComponentSpaceTargetVertices; // size of num vertices of the geometry entry. 
						BuildComponentSpaceVertices(Target.Transform.Get(), Target.Parent.Get(), Target.BoneMap.Get(), Target.Vertex.Get(),
							Target.VertexStart[TargetGeometryIndex], Target.VertexCount[TargetGeometryIndex], ComponentSpaceTargetVertices);

						// Build component space vertices for SampleCollection
						TArray<FVector3f> ComponentSpaceVertices; // size of num vertices of the geometry entry
						BuildComponentSpaceVertices(Sample.Transform.Get(), Sample.Parent.Get(), Sample.BoneMap.Get(), Sample.Vertex.Get(),
							Sample.VertexStart[AttributeGeometryIndex], Sample.VertexCount[AttributeGeometryIndex], ComponentSpaceVertices);

						// build Sphere based BVH
						Chaos::FReal SphereRadius = SphereFullRadius;
						if (SampleScale == EDataflowTransferVertexAttributeNodeSampleScale::Component_Edge)
						{
							SphereRadius = Chaos::FReal(EdgeMultiplier * FMath::Max(
								MaxEdgeLength(ComponentSpaceTargetVertices, Target.Indices.Get(), Target.VertexStart[TargetGeometryIndex], Target.FaceStart[TargetGeometryIndex], Target.FaceCount[TargetGeometryIndex]),
								MaxEdgeLength(ComponentSpaceVertices, Sample.Indices.Get(), Sample.VertexStart[AttributeGeometryIndex], Sample.FaceStart[AttributeGeometryIndex], Sample.FaceCount[AttributeGeometryIndex])));
						}

						int32 TargetVertexStartVal = Target.VertexStart[TargetGeometryIndex];
						int32 TargetVertexCountVal = Target.VertexCount[TargetGeometryIndex];
						int32 VertexStartVal = Sample.VertexStart[AttributeGeometryIndex];
						int32 FaceStartVal = Sample.FaceStart[AttributeGeometryIndex];
						int32 FaceCountVal = Sample.FaceCount[AttributeGeometryIndex];

						if (BoundingVolumeType == EDataflowTransferVertexAttributeNodeBoundingVolume::Triangle)
						{
							TArray<Chaos::TVec3<Chaos::FReal>> ComponentSpaceVerticesTVec3;
							ComponentSpaceVerticesTVec3.SetNum(ComponentSpaceVertices.Num());
							for (int32 SourceIndex = 0; SourceIndex < ComponentSpaceVerticesTVec3.Num(); SourceIndex++)
							{
								ComponentSpaceVerticesTVec3[SourceIndex] = Chaos::TVec3<Chaos::FReal>(ComponentSpaceVertices[SourceIndex]);
							}
							TConstArrayView<Chaos::TVec3<Chaos::FReal>> ConstComponentSpaceVertices(ComponentSpaceVerticesTVec3);
							Chaos::FTriangleMesh TriangleMesh;
							TArray<Chaos::TVec3<int32>> SourceElements;
							SourceElements.SetNum(FaceCountVal);
							for (int32 ElementIndex = 0; ElementIndex < FaceCountVal; ++ElementIndex)
							{
								FIntVector3 Element = Sample.Indices[FaceStartVal + ElementIndex];
								SourceElements[ElementIndex] = Chaos::TVec3<int32>(Element[0] - VertexStartVal, Element[1] - VertexStartVal, Element[2] - VertexStartVal);
							}
							TriangleMesh.Init(SourceElements);
							Chaos::FTriangleMesh::TSpatialHashType<Chaos::FReal> SpatialHash;
							TriangleMesh.BuildSpatialHash(ConstComponentSpaceVertices, SpatialHash, SphereRadius);
							for (int32 TargetIndex = 0; TargetIndex < TargetVertexCountVal; TargetIndex++)
							{
								TArray<Chaos::TTriangleCollisionPoint<Chaos::FReal>> Result;
								if (TriangleMesh.PointClosestTriangleQuery(SpatialHash, ConstComponentSpaceVertices,
									TargetIndex, Chaos::TVec3<Chaos::FReal>(ComponentSpaceTargetVertices[TargetIndex]), SphereRadius / 2.f, SphereRadius / 2.f,
									[](const int32 PointIndex, const int32 TriangleIndex)->bool {return true; }, Result))
								{
									for (const Chaos::TTriangleCollisionPoint<Chaos::FReal>& CollisionPoint : Result)
									{
										float CurrentDistance = abs(CollisionPoint.Phi);
										float TriRadius = FalloffThreshold * MaxEdgeLength(ComponentSpaceVertices, Sample.Indices.Get(), VertexStartVal, FaceStartVal + CollisionPoint.Indices[1], 1);
										float FalloffScale = CalculateFalloffScale(Falloff, TriRadius, CurrentDistance);
										if (!FMath::IsNearlyZero(FalloffScale))
										{
											int32 TargetCandidateIndex = CollisionPoint.Indices[0] + TargetVertexStartVal;
											if (ensure(0 <= TargetCandidateIndex && TargetCandidateIndex < TargetAttributeArray->Num()))
											{
												for (int32 k = 0; k < 3; k++)
												{
													(*TargetAttributeArray)[TargetCandidateIndex] += FalloffScale * (CollisionPoint.Bary[k+1] * (*SourceAttributeArray)[Sample.Indices[FaceStartVal + CollisionPoint.Indices[1]][k]]);
												}
												break;
											}
										}
									}
								}
							}
						}
						else if (BoundingVolumeType == EDataflowTransferVertexAttributeNodeBoundingVolume::Vertex)
						{
							TUniquePtr<BVH> VertexBVH(BuildParticleSphereBVH(ComponentSpaceTargetVertices, SphereRadius));
							for (int32 i = 0; i < FaceCountVal; i++)
							{
								FIntVector3 ComponentTriangle = Sample.Indices[FaceStartVal + i] - FIntVector3(VertexStartVal);
								TArray<int32> TargetVertexIntersection({});
								TriangleToVertexIntersections(*VertexBVH, ComponentSpaceVertices, ComponentTriangle, TargetVertexIntersection);

								for (int32 j = 0; j < TargetVertexIntersection.Num(); j++)
								{
									Chaos::TVector<float, 3> Bary,
										TriPos0(ComponentSpaceVertices[ComponentTriangle[0]]),
										TriPos1(ComponentSpaceVertices[ComponentTriangle[1]]),
										TriPos2(ComponentSpaceVertices[ComponentTriangle[2]]),
										ParticlePos(ComponentSpaceTargetVertices[TargetVertexIntersection[j]]);

									Chaos::TVector<float, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
									float CurrentDistance = (ParticlePos - ClosestPoint).Size();
									float TriRadius = FalloffThreshold * MaxEdgeLength(ComponentSpaceVertices, Sample.Indices.Get(), VertexStartVal, FaceStartVal + i, 1);
									float FalloffScale = CalculateFalloffScale(Falloff, TriRadius, CurrentDistance);
									if (!FMath::IsNearlyZero(FalloffScale))
									{
										int32 TargetIndex = TargetVertexIntersection[j] + TargetVertexStartVal;
										if (ensure(0 <= TargetIndex && TargetIndex < TargetAttributeArray->Num()))
										{
											for (int32 k = 0; k < 3; k++)
											{
												(*TargetAttributeArray)[TargetIndex] += FalloffScale * (Bary[k] * (*SourceAttributeArray)[ComponentTriangle[k] + VertexStartVal]);
											}
											break;
										}
									}
								}
							}
						}
					}
				}
			});
	}
}

template<typename T>
void FGeometryCollectionTransferVertexAttributeNode::NearestVertexTransfer(FCollectionAttributeKey Key, const UE::Private::FTransferFacade& Sample, UE::Private::FTransferFacade& Target, const TManagedArray<T>* SourceAttributeArray, TManagedArray<T>* TargetAttributeArray) const
{
	//TargetAttributeArray has to be filled with empty values before (attribute types: float, FLinearColor)
	if (SourceAttributeArray && TargetAttributeArray)
	{
		// Build component space vertices for TargetCollection
		TArray<FVector3f> ComponentSpaceTargetVertices;
		BuildComponentSpaceVertices(Target.Transform.Get(), Target.Parent.Get(), Target.BoneMap.Get(), Target.Vertex.Get(), 0, Target.Vertex.Num(), ComponentSpaceTargetVertices);

		// Build component space vertices for SourceCollection
		TArray<FVector3f> ComponentSpaceVertices;
		BuildComponentSpaceVertices(Sample.Transform.Get(), Sample.Parent.Get(), Sample.BoneMap.Get(), Sample.Vertex.Get(), 0, Sample.Vertex.Num(), ComponentSpaceVertices);

		// build Sphere based BVH
		Chaos::FReal SphereRadius = Chaos::FReal(EdgeMultiplier * FMath::Max(
			MaxEdgeLength(ComponentSpaceTargetVertices, Target.Indices.Get(), 0, 0, Target.Indices.Num()),
			MaxEdgeLength(ComponentSpaceVertices, Sample.Indices.Get(), 0, 0, Sample.Indices.Num())));
		TUniquePtr<BVH> VertexBVH(BuildParticleSphereBVH(ComponentSpaceTargetVertices, SphereRadius));

		for (int32 i = 0; i < Sample.Indices.Num(); i++)
		{
			FIntVector3 Triangle = Sample.Indices[i];
			TArray<int32> TargetVertexIntersection({});
			TriangleToVertexIntersections(*VertexBVH, ComponentSpaceVertices, Triangle, TargetVertexIntersection);

			for (int32 j = 0; j < TargetVertexIntersection.Num(); j++)
			{
				Chaos::TVector<float, 3> Bary,
					TriPos0(ComponentSpaceVertices[Sample.Indices[i][0]]),
					TriPos1(ComponentSpaceVertices[Sample.Indices[i][1]]),
					TriPos2(ComponentSpaceVertices[Sample.Indices[i][2]]),
					ParticlePos(ComponentSpaceTargetVertices[TargetVertexIntersection[j]]);

				Chaos::TVector<float, 3> ClosestPoint = Chaos::FindClosestPointAndBaryOnTriangle(TriPos0, TriPos1, TriPos2, ParticlePos, Bary);
				float CurrentDistance = (ParticlePos - ClosestPoint).Size();
				float TriRadius = FalloffThreshold * MaxEdgeLength(ComponentSpaceVertices, Sample.Indices.Get(), 0, i, 1);
				float FalloffScale = CalculateFalloffScale(Falloff, TriRadius, CurrentDistance);
				if (!FMath::IsNearlyZero(FalloffScale))
				{
					int32 TargetIndex = TargetVertexIntersection[j];
					if (ensure(0 <= TargetIndex && TargetIndex < TargetAttributeArray->Num()))
					{
						for (int32 k = 0; k < 3; k++)
						{
							(*TargetAttributeArray)[TargetIndex] += FalloffScale * (Bary[k] * (*SourceAttributeArray)[Sample.Indices[i][k]]);
						}
						break;
					}
				}
			}
		}
	}
}


float FGeometryCollectionTransferVertexAttributeNode::MaxEdgeLength(TArray<FVector3f>& Vert, const TManagedArray<FIntVector3>& Tri, int VertexOffset, int TriStart, int TriCount)
{
	auto TriInRange = [](const FIntVector3& T, int Max) {
		for (int k = 0; k < 3; k++)
		{
			if (ensure(0 <= T[k] && T[k] < Max))
			{
				return true;
			}
		}
		return false;
		};

	float Max = 0;
	int TriStop = TriStart + TriCount;
	for (int i = TriStart; i < TriStop; i++)
	{
		if (TriInRange(Tri[i] - FIntVector3(VertexOffset), Vert.Num()))
		{
			Max = FMath::Max(Max, (Vert[Tri[i][0] - VertexOffset] - Vert[Tri[i][1] - VertexOffset]).SquaredLength());
			Max = FMath::Max(Max, (Vert[Tri[i][0] - VertexOffset] - Vert[Tri[i][2] - VertexOffset]).SquaredLength());
			Max = FMath::Max(Max, (Vert[Tri[i][1] - VertexOffset] - Vert[Tri[i][2] - VertexOffset]).SquaredLength());
		}
	}
	return FMath::Sqrt(Max);
}

void FGeometryCollectionTransferVertexAttributeNode::BuildComponentSpaceVertices(const TManagedArray<FTransform3f>& LocalSpaceTransform, const TManagedArray<int32>& Parent, const TManagedArray<int32>& BoneMapArray,
	const TManagedArray<FVector3f>& VertexArray, int32 Start, int32 Count, TArray<FVector3f>& ComponentSpaceVertices)
{
	TArray<FTransform3f> ComponentTransform;
	GeometryCollectionAlgo::GlobalMatrices(LocalSpaceTransform, Parent, ComponentTransform);

	ComponentSpaceVertices.SetNumUninitialized(Count);
	for (int i = 0; i < Count; i++)
	{
		int j = i + Start;
		if (0 < (BoneMapArray)[i] && (BoneMapArray)[i] < ComponentTransform.Num())
		{
			ComponentSpaceVertices[i] = ComponentTransform[(BoneMapArray)[j]].TransformPosition(VertexArray[j]);
		}
		else
		{
			ComponentSpaceVertices[i] = VertexArray[j];
		}
	}
}

FGeometryCollectionTransferVertexAttributeNode::BVH* FGeometryCollectionTransferVertexAttributeNode::BuildParticleSphereBVH(const TArray<FVector3f>& Vertices, float Radius)
{
	TArray<SphereType*> VertexSpherePtrs;
	TArray<SphereType> VertexSpheres;
	VertexSpheres.Init(SphereType(Chaos::FVec3f(0), Radius), Vertices.Num());
	VertexSpherePtrs.SetNum(Vertices.Num());

	for (int32 i = 0; i < Vertices.Num(); i++)
	{
		Chaos::FVec3f SphereCenter(Vertices[i]);
		SphereType VertexSphere(SphereCenter, Radius);
		VertexSpheres[i] = SphereType(SphereCenter, Radius);
		VertexSpherePtrs[i] = &VertexSpheres[i];
	}
	return new BVH(VertexSpherePtrs);
}

void FGeometryCollectionTransferVertexAttributeNode::TriangleToVertexIntersections(
	const BVH& VertexBVH, const TArray<FVector3f>& ComponentSpaceVertices, const FIntVector3& Triangle, TArray<int32>& OutTargetVertexIntersection)
{
	OutTargetVertexIntersection.Empty();

	TArray<int32> TargetVertexIntersection0 = VertexBVH.FindAllIntersections(ComponentSpaceVertices[Triangle[0]]);
	TArray<int32> TargetVertexIntersection1 = VertexBVH.FindAllIntersections(ComponentSpaceVertices[Triangle[1]]);
	TArray<int32> TargetVertexIntersection2 = VertexBVH.FindAllIntersections(ComponentSpaceVertices[Triangle[2]]);
	TargetVertexIntersection0.Sort();
	TargetVertexIntersection1.Sort();
	TargetVertexIntersection2.Sort();

	for (int32 k = 0; k < TargetVertexIntersection0.Num(); k++)
	{
		if (TargetVertexIntersection1.Contains(TargetVertexIntersection0[k])
			&& TargetVertexIntersection2.Contains(TargetVertexIntersection0[k]))
		{
			OutTargetVertexIntersection.Emplace(TargetVertexIntersection0[k]);
		}
	}
}


float FGeometryCollectionTransferVertexAttributeNode::CalculateFalloffScale(EDataflowTransferVertexAttributeNodeFalloff FalloffSetting, float Threshold, float Distance)
{
	float Denominator = 1.0;
	if (Distance > Threshold && !FMath::IsNearlyZero(Threshold))
	{
		Denominator = Distance / Threshold;
	}
	switch (FalloffSetting)
	{
	case EDataflowTransferVertexAttributeNodeFalloff::Linear:
		return 1. / Denominator;
	case EDataflowTransferVertexAttributeNodeFalloff::Squared:
		return 1. / FMath::Square(Denominator);
	}
	return 1.0;
}


#undef LOCTEXT_NAMESPACE
