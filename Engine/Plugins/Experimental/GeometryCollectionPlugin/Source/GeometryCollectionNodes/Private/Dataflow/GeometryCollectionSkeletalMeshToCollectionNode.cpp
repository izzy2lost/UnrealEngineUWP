// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionSkeletalMeshToCollectionNode.h"

#include "Engine/SkeletalMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "MeshDescriptionToDynamicMesh.h"

#include "MeshDescription.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "IndexTypes.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"

void FSkeletalMeshToCollectionDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		FGeometryCollection OutCollection;
		TObjectPtr<const USkeletalMesh> InSkeletalMesh = GetValue<TObjectPtr<const USkeletalMesh>>(Context, &SkeletalMesh);
		if (InSkeletalMesh)
		{
			if (Method == EDF_ConversionMethod::Skinned)
			{
				FGeometryCollectionEngineConversion::AppendSkeletalMesh(InSkeletalMesh, 0, FTransform::Identity, &OutCollection);
			}
			else if (Method == EDF_ConversionMethod::StaticComponents)
			{
				AppendSkeletalMeshComponentsToGeometryCollection(InSkeletalMesh, OutCollection);
			}
		}
		FManagedArrayCollection OutManagedArrayCollection = OutCollection;
		SetValue(Context, MoveTemp(OutManagedArrayCollection), &Collection);
	}
}




void FSkeletalMeshToCollectionDataflowNode::AppendSkeletalMeshComponentsToGeometryCollection(const USkeletalMesh* InSkeletalMesh, FGeometryCollection& OutCollection) const
{
#if WITH_EDITORONLY_DATA
	UE::Geometry::FDynamicMesh3 DynamicMesh;

	// Check first if we have bulk data available and non-empty.
	constexpr int32 LODIndex = 0;
	FMeshDescription SourceMesh;
	if (InSkeletalMesh->HasMeshDescription(LODIndex))
	{
		InSkeletalMesh->CloneMeshDescription(LODIndex, SourceMesh);
	}
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(&SourceMesh, DynamicMesh);

	//
	// Compute by Component
	//
	bool bComputeByComponent = true;
	if (bComputeByComponent)
	{
		TArray<TArray<int32>> ConnectedComponents;

		TArray<FIntVector3> Faces;
		Faces.SetNum(DynamicMesh.TriangleCount());
		for (int32 i = 0; i < DynamicMesh.TriangleCount(); ++i)
		{
			Faces[i] = FIntVector3(DynamicMesh.GetTriangle(i));
		}
		Chaos::Utilities::FindConnectedRegions(Faces, ConnectedComponents);

		TArray<TUniquePtr<FGeometryCollection>> CollectionBuffer;
		for (int32 i = 0; i < ConnectedComponents.Num(); i++)
		{
			CollectionBuffer.Add(nullptr);
		}
		ParallelFor(ConnectedComponents.Num(),[&](int32 i)
		{
			UE::Geometry::FDynamicMesh3 Component;

			for (FVector V : DynamicMesh.VerticesItr()) {
				Component.AppendVertex(V);
			}
			for (int32 j = 0; j < ConnectedComponents[i].Num(); j++) {
				int32 ElementIndex = ConnectedComponents[i][j];
				for (int32 ie = 0; ie < 3; ie++) {
					Component.AppendTriangle(Faces[ElementIndex][0], Faces[ElementIndex][1], Faces[ElementIndex][2]);
				}
			}
			Component.CompactInPlace();
			Component.ReverseOrientation();

			int Idx = 0;
			TArray<float> VertexArray;
			VertexArray.SetNumUninitialized(Component.VertexCount() * 3);
			for (FVector V : Component.VerticesItr())
			{
				VertexArray[Idx++] = V[0];
				VertexArray[Idx++] = V[1];
				VertexArray[Idx++] = V[2];
			}

			Idx = 0;
			TArray<int> IndicesArray;
			IndicesArray.SetNumUninitialized(Component.TriangleCount() * 3);
			for (UE::Geometry::FIndex3i Tri : Component.TrianglesItr())
			{
				IndicesArray[Idx++] = Tri[0];
				IndicesArray[Idx++] = Tri[1];
				IndicesArray[Idx++] = Tri[2];
			}

			CollectionBuffer[i] = TUniquePtr<FGeometryCollection>(FGeometryCollection::NewGeometryCollection(VertexArray, IndicesArray));
		});

		for (int32 i = 0; i < CollectionBuffer.Num(); i++)
		{
			if (CollectionBuffer[i]->NumElements(FGeometryCollection::VerticesGroup))
			{
				TSet<int32> VertexToDeleteSet;
				GeometryCollectionAlgo::ComputeStaleVertices(CollectionBuffer[i].Get(), VertexToDeleteSet);
				TArray<int32> SortedVertices = VertexToDeleteSet.Array(); SortedVertices.Sort();
				if (VertexToDeleteSet.Num()) CollectionBuffer[i]->RemoveElements(FGeometryCollection::VerticesGroup, SortedVertices);

				int32 GeomIndex = OutCollection.AppendGeometry(*CollectionBuffer[i].Get());
			}
		}
	}
#endif
}

