// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshCreateTetrahedronNode.h"
#include "Dataflow/ChaosFleshEngineAssetNodes.h"

#include "Async/ParallelFor.h"
#include "Chaos/Deformable/Utilities.h"
#include "ChaosFlesh/ChaosFlesh.h"
#include "Chaos/Tetrahedron.h"
#include "Chaos/Utilities.h"
#include "Chaos/UniformGrid.h"
#include "ChaosFlesh/FleshCollection.h"
#include "ChaosFlesh/FleshCollectionUtility.h"
#include "ChaosLog.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/ChaosFleshNodesUtility.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "FTetWildWrapper.h"
#include "Generate/IsosurfaceStuffing.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/Facades/CollectionTetrahedralMetricsFacade.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshLODModelToDynamicMesh.h" // MeshModelingBlueprints
#include "Spatial/FastWinding.h"
#include "Spatial/MeshAABBTree3.h"
#include "Dataflow/ChaosFleshNodesUtility.h"

//=============================================================================
// FCreateTetrahedronDataflowNode
//=============================================================================


void FCreateTetrahedronDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		FFleshCollection GeneratedTetrahedron;
		TUniquePtr<FFleshCollection> InCollection(GetValue<DataType>(Context, &Collection).NewCopy<FFleshCollection>());

		// TransformGroup
		int32 NumTransforms = InCollection->NumElements(FTransformCollection::TransformGroup);
		TManagedArray<FString>* TransformName = InCollection->FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
		TManagedArray<int32>* TransformToGeometryIndex = InCollection->FindAttribute<int32>("TransformToGeometryIndex", FTransformCollection::TransformGroup);
		// Geometry Group
		int32 NumGeometry = InCollection->NumElements(FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* GroupToTransformIndex = InCollection->FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* VertexCount = InCollection->FindAttribute<int32>("VertexCount", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* VertexStart = InCollection->FindAttribute<int32>("VertexStart", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* FaceCount = InCollection->FindAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* FaceStart = InCollection->FindAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);
		// Vertices Group
		int32 NumVertices = InCollection->NumElements(FGeometryCollection::VerticesGroup);
		const TManagedArray<FVector3f>* Vertex = InCollection->FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
		// Faces Group
		int32 NumTriangles = InCollection->NumElements(FGeometryCollection::FacesGroup);
		const TManagedArray<FIntVector>* Faces = InCollection->FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);

		if (TransformName && TransformToGeometryIndex && GroupToTransformIndex && VertexCount && VertexStart && FaceCount && FaceStart
			&& Vertex && Faces)
		{
			TArray<int32> ProcessGeometryIndices = Dataflow::GetMatchingMeshIndices(MeshNames, InCollection.Get());

			TArray<TUniquePtr<FFleshCollection>> CollectionBuffer;
			for (int32 Gdx = 0; Gdx < NumGeometry; Gdx++)
			{
				if (ProcessGeometryIndices.Contains(Gdx)) 
					CollectionBuffer.Add(TUniquePtr<FFleshCollection>( new FFleshCollection()));
				else 
					CollectionBuffer.Add(nullptr);
			}

			ParallelFor(ProcessGeometryIndices.Num(), [&](int32 i)
			{
				int32 Gdx = ProcessGeometryIndices[i];
				FFleshCollection& TetCollection = *CollectionBuffer[Gdx];

				UE::Geometry::FDynamicMesh3 DynamicMesh;
				int32 vStart = (*VertexStart)[Gdx], vEnd = vStart + (*VertexCount)[Gdx];
				for (int Vdx = vStart; Vdx < vEnd; Vdx++) DynamicMesh.AppendVertex(FVector((*Vertex)[Vdx]));
				int32 fStart = (*FaceStart)[Gdx], fEnd = fStart + (*FaceCount)[Gdx];
				for (int Fdx = fStart; Fdx < fEnd; Fdx++) DynamicMesh.AppendTriangle((*Faces)[Fdx] - FIntVector(vStart));
				DynamicMesh.CompactInPlace();

				if (Method == TetMeshingMethod::IsoStuffing)
				{
					EvaluateIsoStuffing(Context, TetCollection, DynamicMesh);
				}
				else if (Method == TetMeshingMethod::TetWild)
				{
					EvaluateTetWild(Context, TetCollection, DynamicMesh);
				}

				if (TetCollection.NumElements(FGeometryCollection::VerticesGroup))
				{
					TSet<int32> VertexToDeleteSet;
					GeometryCollectionAlgo::ComputeStaleVertices(&TetCollection, VertexToDeleteSet);
					TArray<int32> SortedVertices = VertexToDeleteSet.Array(); SortedVertices.Sort();
					if (VertexToDeleteSet.Num()) TetCollection.RemoveElements(FGeometryCollection::VerticesGroup, SortedVertices);
				}
			});

			auto AppendProcessedGeometry = [&ProcessGeometryIndices, &CollectionBuffer, &InCollection](FFleshCollection& ToCollection)
			{
				TManagedArray<int32>* SourceGroupToTransformIndex = InCollection->FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
				TManagedArray<FString>* SourceTransformName = InCollection->FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
				TManagedArray<int32>* ToGroupToTransformIndex = ToCollection.FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
				TManagedArray<FString>* ToTransformName = ToCollection.FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);

				if (SourceGroupToTransformIndex && SourceTransformName && ToGroupToTransformIndex && ToTransformName)
				{
					for (int32 Sdx = 0; Sdx < ProcessGeometryIndices.Num(); Sdx++)
					{
						int32 Gdx = ProcessGeometryIndices[Sdx];
						if (CollectionBuffer[Gdx])
						{
							int32 GeomIndex = ToCollection.NumElements(FGeometryCollection::GeometryGroup);
							ToCollection.AppendGeometry(*CollectionBuffer[Gdx]);
							// source data
							int32 SourceNumTransforms = InCollection->NumElements(FGeometryCollection::TransformGroup);
							int32 SourceTransformIndex = (*SourceGroupToTransformIndex)[Gdx];
							// target data
							int32 ToNumTransforms = ToCollection.NumElements(FGeometryCollection::TransformGroup);
							int32 ToGeomTransformIndex = (*ToGroupToTransformIndex)[GeomIndex];

							FString TetName = FString::Printf(TEXT("Tet%d"), GeomIndex);
							if (0 <= SourceTransformIndex && SourceTransformIndex < SourceNumTransforms)
							{
								if (!(*SourceTransformName)[SourceTransformIndex].IsEmpty())
								{
									TetName = FString::Printf(TEXT("%s_%s"), *(*SourceTransformName)[SourceTransformIndex], *TetName);
								}
							}
							if (0 <= ToGeomTransformIndex && ToGeomTransformIndex < ToNumTransforms)
							{
								(*ToTransformName)[ToGeomTransformIndex] = TetName;
							}
						}
					}
				}
			};
			AppendProcessedGeometry(GeneratedTetrahedron);
		}
		SetValue<const DataType&>(Context, MoveTemp(GeneratedTetrahedron), &Collection);
	}
}

void FCreateTetrahedronDataflowNode::EvaluateIsoStuffing(
	Dataflow::FContext& Context, 
	FFleshCollection& InCollection,
	const UE::Geometry::FDynamicMesh3& DynamicMesh) const
{
#if WITH_EDITORONLY_DATA
	if (NumCells > 0 && (-.5 <= OffsetPercent && OffsetPercent <= 0.5))
	{
		// Tet mesh generation
		UE::Geometry::TIsosurfaceStuffing<double> IsosurfaceStuffing;
		UE::Geometry::FDynamicMeshAABBTree3 Spatial(&DynamicMesh);
		UE::Geometry::TFastWindingTree<UE::Geometry::FDynamicMesh3> FastWinding(&Spatial);
		UE::Geometry::FAxisAlignedBox3d Bounds = Spatial.GetBoundingBox();
		IsosurfaceStuffing.Bounds = FBox(Bounds);
		double CellSize = Bounds.MaxDim() / NumCells;
		IsosurfaceStuffing.CellSize = CellSize;
		IsosurfaceStuffing.IsoValue = .5 + OffsetPercent;
		IsosurfaceStuffing.Implicit = [&FastWinding, &Spatial](FVector3d Pos)
		{
			FVector3d Nearest = Spatial.FindNearestPoint(Pos);
			double WindingSign = FastWinding.FastWindingNumber(Pos) - .5;
			return FVector3d::Distance(Nearest, Pos) * FMathd::SignNonZero(WindingSign);
		};

		UE_LOG(LogChaosFlesh, Display, TEXT("Generating tet mesh via IsoStuffing..."));
		IsosurfaceStuffing.Generate();
		if (IsosurfaceStuffing.Tets.Num() > 0)
		{
			TArray<FVector> Vertices; Vertices.SetNumUninitialized(IsosurfaceStuffing.Vertices.Num());
			TArray<FIntVector4> Elements; Elements.SetNumUninitialized(IsosurfaceStuffing.Tets.Num());
			TArray<FIntVector3> SurfaceElements = Dataflow::GetSurfaceTriangles(IsosurfaceStuffing.Tets, !bDiscardInteriorTriangles);

			for (int32 Tdx = 0; Tdx < IsosurfaceStuffing.Tets.Num(); ++Tdx)
			{
				Elements[Tdx] = IsosurfaceStuffing.Tets[Tdx];
			}
			for (int32 Vdx = 0; Vdx < IsosurfaceStuffing.Vertices.Num(); ++Vdx)
			{
				Vertices[Vdx] = IsosurfaceStuffing.Vertices[Vdx];
			}

			TUniquePtr<FTetrahedralCollection> TetCollection(FTetrahedralCollection::NewTetrahedralCollection(Vertices, SurfaceElements, Elements));
			InCollection.AppendGeometry(*TetCollection.Get());

			UE_LOG(LogChaosFlesh, Display,
				TEXT("Generated tet mesh via IsoStuffing, num vertices: %d num tets: %d"), Vertices.Num(), Elements.Num());
		}
		else
		{
			UE_LOG(LogChaosFlesh, Warning, TEXT("IsoStuffing produced 0 tetrahedra."));
		}
	}
#else
	ensureMsgf(false, TEXT("FCreateTetrahedronDataflowNodes is an editor only node."));
#endif
}

void FCreateTetrahedronDataflowNode::EvaluateTetWild(
	Dataflow::FContext& Context, 
	FFleshCollection& InCollection,
	const UE::Geometry::FDynamicMesh3& DynamicMesh) const
{
#if WITH_EDITORONLY_DATA
	if (/* placeholder for conditions for exec */true)
	{
		// Pull out Vertices and Triangles
		TArray<FVector> Verts;
		TArray<FIntVector3> Tris;
		for (FVector V : DynamicMesh.VerticesItr())
		{
			Verts.Add(V);
		}
		for (UE::Geometry::FIndex3i Tri : DynamicMesh.TrianglesItr())
		{
			Tris.Emplace(Tri.A, Tri.B, Tri.C);
		}

		// Tet mesh generation
		UE::Geometry::FTetWild::FTetMeshParameters Params;
		Params.bCoarsen = bCoarsen;
		Params.bExtractManifoldBoundarySurface = bExtractManifoldBoundarySurface;
		Params.bSkipSimplification = bSkipSimplification;

		Params.EpsRel = EpsRel;
		Params.MaxIts = MaxIterations;
		Params.StopEnergy = StopEnergy;
		Params.IdealEdgeLength = IdealEdgeLength;

		Params.bInvertOutputTets = bInvertOutputTets;

		TArray<FVector> TetVerts;
		TArray<FIntVector4> Tets;
		FProgressCancel Progress;
		UE_LOG(LogChaosFlesh, Display,TEXT("Generating tet mesh via TetWild..."));
		if (UE::Geometry::FTetWild::ComputeTetMesh(Params, Verts, Tris, TetVerts, Tets, &Progress))
		{
			TArray<FIntVector3> SurfaceElements = Dataflow::GetSurfaceTriangles(Tets, !bDiscardInteriorTriangles);
			TUniquePtr<FTetrahedralCollection> TetCollection(FTetrahedralCollection::NewTetrahedralCollection(TetVerts, SurfaceElements, Tets));
			InCollection.AppendGeometry(*TetCollection.Get());

			UE_LOG(LogChaosFlesh, Display,
				TEXT("Generated tet mesh via TetWild, num vertices: %d num tets: %d"), TetVerts.Num(), Tets.Num());
		}
		else
		{
			UE_LOG(LogChaosFlesh, Error,
				TEXT("TetWild tetrahedral mesh generation failed."));
		}
	}
#else
	ensureMsgf(false, TEXT("FCreateTetrahedronDataflowNodes is an editor only node."));
#endif
}
