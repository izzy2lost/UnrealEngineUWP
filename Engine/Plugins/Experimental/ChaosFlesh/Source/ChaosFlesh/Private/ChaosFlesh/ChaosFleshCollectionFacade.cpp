// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosFlesh/ChaosFleshCollectionFacade.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/TransformCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "ChaosFlesh/TetrahedralCollection.h"

#define LOCTEXT_NAMESPACE "FFleshCollectionFacade"

namespace Chaos 
{

	FFleshCollectionFacade::FFleshCollectionFacade(FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(&InCollection)
		, BoneName(InCollection, "BoneName", FTransformCollection::TransformGroup)
		, Transform(InCollection, FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup)
		, Parent(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
		, Child(InCollection, FTransformCollection::ChildrenAttribute, FTransformCollection::TransformGroup)
		, BoneMap(InCollection, FName("BoneMap"), FName("Vertices"))
		, Vertex(InCollection, FName("Vertex"), FName("Vertices"))
		, Indices(InCollection, FName("Indices"), FName("Faces"))
		, Tetrahedron(InCollection, FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup)
		, VertexStart(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCount(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
		, FaceStart(InCollection, "FaceStart", FGeometryCollection::GeometryGroup)
		, FaceCount(InCollection, "FaceCount", FGeometryCollection::GeometryGroup)
	{}

	FFleshCollectionFacade::FFleshCollectionFacade(const FManagedArrayCollection& InCollection)
		: ConstCollection(InCollection)
		, Collection(nullptr)
		, BoneName(InCollection, "BoneName", FTransformCollection::TransformGroup)
		, Transform(InCollection, FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup)
		, Parent(InCollection, FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup)
		, Child(InCollection, FTransformCollection::ChildrenAttribute, FTransformCollection::TransformGroup)
		, BoneMap(InCollection, FName("BoneMap"), FName("Vertices"))
		, Vertex(InCollection, FName("Vertex"), FName("Vertices"))
		, Indices(InCollection, FName("Indices"), FName("Faces"))
		, Tetrahedron(InCollection, FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup)
		, VertexStart(InCollection, "VertexStart", FGeometryCollection::GeometryGroup)
		, VertexCount(InCollection, "VertexCount", FGeometryCollection::GeometryGroup)
		, FaceStart(InCollection, "FaceStart", FGeometryCollection::GeometryGroup)
		, FaceCount(InCollection, "FaceCount", FGeometryCollection::GeometryGroup)
	{}

	bool FFleshCollectionFacade::IsCompletelyValid() const {
		return BoneName.IsValid() && Transform.IsValid() && Parent.IsValid() && Child.IsValid() &&
			BoneMap.IsValid() && Vertex.IsValid() &&
			Indices.IsValid() && Tetrahedron.IsValid() &&
			VertexStart.IsValid() && VertexCount.IsValid() && FaceStart.IsValid() && FaceCount.IsValid();
	}

	bool FFleshCollectionFacade::IsTetrahedronValid() const {
		return Vertex.IsValid() && Tetrahedron.IsValid();
	}

	bool FFleshCollectionFacade::IsHierarchyValid() const {
		return Transform.IsValid() && Parent.IsValid();
	}

	void FFleshCollectionFacade::ComponentSpaceVertices(TArray<FVector3f>& OutComponentSpaceVertices)
	{
		ComponentSpaceVertices(OutComponentSpaceVertices, 0, Vertex.Num());
	}

	void FFleshCollectionFacade::ComponentSpaceVertices(TArray<FVector3f>& OutComponentSpaceVertices, int32 Start, int32 Count)
	{
		if (IsHierarchyValid() && Vertex.IsValid())
		{
			TArray<FTransform3f> ComponentTransform;
			GeometryCollectionAlgo::GlobalMatrices(Transform.Get(), Parent.Get(), ComponentTransform);

			OutComponentSpaceVertices.SetNumUninitialized(Count);
			for (int i = 0; i < Count; i++)
			{
				int j = i + Start;
				if (0 < BoneMap[i] && BoneMap[i] < ComponentTransform.Num())
				{
					OutComponentSpaceVertices[i] = ComponentTransform[BoneMap[j]].TransformPosition(Vertex[j]);
				}
				else
				{
					OutComponentSpaceVertices[i] = Vertex[j];
				}
			}
		}
	}

}
