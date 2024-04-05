// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshTetrahedralNodes.h"
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

namespace Dataflow
{
	void ChaosFleshTetrahedralNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCalculateTetMetrics);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateTetrahedralCollectionDataflowNodes);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FConstructTetGridNode);
	}

}


//=============================================================================
// FCalculateTetMetrics
//=============================================================================

void
FCalculateTetMetrics::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		TUniquePtr<FFleshCollection> InCollection(GetValue<DataType>(Context, &Collection).NewCopy<FFleshCollection>());

		TManagedArray<FIntVector4>* TetMesh =
			InCollection->FindAttribute<FIntVector4>(
				FTetrahedralCollection::TetrahedronAttribute, FTetrahedralCollection::TetrahedralGroup);
		TManagedArray<int32>* TetrahedronStart =
			InCollection->FindAttribute<int32>(
				FTetrahedralCollection::TetrahedronStartAttribute, FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* TetrahedronCount =
			InCollection->FindAttribute<int32>(
				FTetrahedralCollection::TetrahedronCountAttribute, FGeometryCollection::GeometryGroup);

		TManagedArray<FVector3f>* Vertex =
			InCollection->FindAttribute<FVector3f>(
				"Vertex", "Vertices");

		GeometryCollection::Facades::FTetrahedralMetrics TetMetrics(*InCollection);
		TManagedArrayAccessor<float>& SignedVolume = TetMetrics.GetSignedVolume();
		TManagedArrayAccessor<float>& AspectRatio = TetMetrics.GetAspectRatio();

		for (int32 TetMeshIdx = 0; TetMeshIdx < TetrahedronStart->Num(); TetMeshIdx++)
		{
			const int32 TetMeshStart = (*TetrahedronStart)[TetMeshIdx];
			const int32 TetMeshCount = (*TetrahedronCount)[TetMeshIdx];
			
			float MinVol = TNumericLimits<float>::Max();
			float MaxVol = -TNumericLimits<float>::Max();
			double AvgVol = 0.0;

			float MinAR = TNumericLimits<float>::Max();
			float MaxAR = -TNumericLimits<float>::Max();
			double AvgAR = 0.0;
			
			for (int32 i = 0; i < TetMeshCount; i++)
			{
				const int32 Idx = TetMeshStart + i;
				const FIntVector4& Tet = (*TetMesh)[Idx];
				Chaos::TTetrahedron<Chaos::FReal> Tetrahedron(
					(*Vertex)[Tet[0]],
					(*Vertex)[Tet[1]],
					(*Vertex)[Tet[2]],
					(*Vertex)[Tet[3]]);

				float Vol = Tetrahedron.GetSignedVolume();
				SignedVolume.ModifyAt(Idx, Vol);
				MinVol = MinVol < Vol ? MinVol : Vol;
				MaxVol = MaxVol < Vol ? Vol : MaxVol;
				AvgVol += Vol;

				float AR = Tetrahedron.GetAspectRatio();
				AspectRatio.ModifyAt(Idx, AR);
				MinAR = MinAR < AR ? MinAR : AR;
				MaxAR = MaxAR < AR ? AR : MaxAR;
				AvgAR += AR;
			}
			if (TetMeshCount)
			{
				AvgVol /= TetMeshCount;
				AvgAR /= TetMeshCount;
			}
			else
			{
				MinVol = MaxVol = 0.0f;
				MinAR = MaxAR = 0.0f;
			}

			UE_LOG(LogChaosFlesh, Display,
				TEXT("'%s' - Tet mesh %d of %d stats:\n"
				"    Num Tetrahedra: %d\n"
				"    Volume (min, avg, max): %g, %g, %g\n"
				"    Aspect ratio (min, avg, max): %g, %g, %g"),
				*GetName().ToString(),
				(TetMeshIdx+1), TetrahedronStart->Num(),
				TetMeshCount,
				MinVol, AvgVol, MaxVol,
				MinAR, AvgAR, MaxAR);
		}

		SetValue<const DataType&>(Context, *InCollection, &Collection);
	}
}

//=============================================================================
// FConstructTetGridNode
//=============================================================================

void FConstructTetGridNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		TUniquePtr<FFleshCollection> InCollection(GetValue<DataType>(Context, &Collection).NewCopy<FFleshCollection>());

		Chaos::TVector<int32, 3> Counts(GridCellCount[0], GridCellCount[1], GridCellCount[2]);

		Chaos::TVector<double, 3> MinCorner = -.5 * GridDomain;
		Chaos::TVector<double, 3> MaxCorner = .5 * GridDomain;
		Chaos::TUniformGrid<double, 3> Grid(MinCorner, MaxCorner, Counts, 0);

		TArray<FIntVector4> Tets;
		TArray<FVector> X;
		Chaos::Utilities::TetMeshFromGrid<double>(Grid, Tets, X);

		UE_LOG(LogChaosFlesh, Display, TEXT("TetGrid generated %d points and %d tetrahedra."), X.Num(), Tets.Num());

		TArray<FIntVector3> Tris = Dataflow::GetSurfaceTriangles(Tets, !bDiscardInteriorTriangles);
		TUniquePtr<FTetrahedralCollection> TetCollection(
			FTetrahedralCollection::NewTetrahedralCollection(X, Tris, Tets));
		InCollection->AppendGeometry(*TetCollection.Get());

		SetValue<const DataType&>(Context, *InCollection, &Collection);
	}
}

//=============================================================================
// FGenerateTetrahedralCollectionDataflowNodes
//=============================================================================


void FGenerateTetrahedralCollectionDataflowNodes::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<DataType>(&Collection))
	{
		TUniquePtr<FFleshCollection> InCollection(GetValue<DataType>(Context, &Collection).NewCopy<FFleshCollection>());
		TObjectPtr<const UStaticMesh> InStaticMesh(GetValue<TObjectPtr<const UStaticMesh>>(Context, &StaticMesh));
		TObjectPtr<const USkeletalMesh> InSkeletalMesh(FindInput(&SkeletalMesh) ? GetValue<TObjectPtr<const USkeletalMesh>>(Context, &SkeletalMesh) : nullptr);

		if (InStaticMesh || InSkeletalMesh)
		{
#if WITH_EDITORONLY_DATA
			UE::Geometry::FDynamicMesh3 DynamicMesh;
			if (InStaticMesh)
			{
				// make a mesh description for UE::Geometry tools
				FMeshDescriptionToDynamicMesh GetSourceMesh;
				bool bUsingHiResSource = InStaticMesh->IsHiResMeshDescriptionValid();
				const FMeshDescription* UseSourceMeshDescription =
					(bUsingHiResSource) ? InStaticMesh->GetHiResMeshDescription() : InStaticMesh->GetMeshDescription(0);
				GetSourceMesh.Convert(UseSourceMeshDescription, DynamicMesh);
			}
			else if (InSkeletalMesh)
			{
				// Check first if we have bulk data available and non-empty.
				constexpr int32 LODIndex = 0;
				FMeshDescription SourceMesh;
				if (InSkeletalMesh->HasMeshDescription(LODIndex))
				{
					InSkeletalMesh->CloneMeshDescription(LODIndex, SourceMesh);
				}
				FMeshDescriptionToDynamicMesh Converter;
				Converter.Convert(&SourceMesh, DynamicMesh);
			}

			if (!bComputeByComponent)
			{
				if (Method == TetMeshingMethod::IsoStuffing)
				{
					EvaluateIsoStuffing(Context, InCollection, DynamicMesh);
				}
				else if (Method == TetMeshingMethod::TetWild)
				{
					EvaluateTetWild(Context, InCollection, DynamicMesh);
				}
				else
				{
					ensureMsgf(false, TEXT("FGenerateTetrahedralCollectionDataflowNodes unsupported Method."));
				}
			}
			else
			{
				TArray<TArray<int32>> ConnectedComponents;
				
				TArray<FIntVector3> Faces;
				Faces.SetNum(DynamicMesh.TriangleCount());
				for (int32 i = 0; i < DynamicMesh.TriangleCount(); ++i)
				{
					Faces[i] = FIntVector3(DynamicMesh.GetTriangle(i));
				}
				Chaos::Utilities::FindConnectedRegions(Faces, ConnectedComponents);
				TArray<TUniquePtr<FFleshCollection>> CollectionBuffer;
				for (int32 i = 0; i < ConnectedComponents.Num(); i++)
				{
					CollectionBuffer.Add(TUniquePtr<FFleshCollection>(new FFleshCollection()));
					//CollectionBuffer[i]->AddElement(1, FGeometryCollection::TransformGroup);
				}
				ParallelFor(ConnectedComponents.Num(), [&](int32 i)
				{		
					UE::Geometry::FDynamicMesh3 ComponentDynamicMesh;
					TArray<FIntVector3> ComponentFaces;
					ComponentFaces.SetNum(ConnectedComponents[i].Num());
					for (FVector V : DynamicMesh.VerticesItr())
					{
						ComponentDynamicMesh.AppendVertex(V);
					}
					for (int32 j = 0; j < ConnectedComponents[i].Num(); j++)
					{
						int32 ElementIndex = ConnectedComponents[i][j];
						for (int32 ie = 0; ie < 3; ie++)
						{
							ComponentDynamicMesh.AppendTriangle(Faces[ElementIndex][0], Faces[ElementIndex][1], Faces[ElementIndex][2]);
						}
					}
					ComponentDynamicMesh.CompactInPlace();
					if (Method == TetMeshingMethod::IsoStuffing)
					{
						EvaluateIsoStuffing(Context, CollectionBuffer[i], ComponentDynamicMesh);
					}
					else if (Method == TetMeshingMethod::TetWild)
					{
						EvaluateTetWild(Context, CollectionBuffer[i], ComponentDynamicMesh);
					}
				});
				for (int32 i = 0; i < ConnectedComponents.Num(); i++)
				{
					if (CollectionBuffer[i]->NumElements(FGeometryCollection::VerticesGroup))
					{
						TSet<int32> VertexToDeleteSet;
						GeometryCollectionAlgo::ComputeStaleVertices(CollectionBuffer[i].Get(), VertexToDeleteSet);
						TArray<int32> SortedVertices = VertexToDeleteSet.Array(); SortedVertices.Sort();
						if (VertexToDeleteSet.Num()) CollectionBuffer[i]->RemoveElements(FGeometryCollection::VerticesGroup, SortedVertices);

						int32 GeomIndex = InCollection->AppendGeometry(*CollectionBuffer[i].Get());
					}
				}
			}
#else
			ensureMsgf(false, TEXT("FGenerateTetrahedralCollectionDataflowNodes is an editor only node."));
#endif
		} // end if InStaticMesh || InSkeletalMesh
		SetValue<const DataType&>(Context, *InCollection, &Collection);
	}
}

void FGenerateTetrahedralCollectionDataflowNodes::EvaluateIsoStuffing(
	Dataflow::FContext& Context, 
	TUniquePtr<FFleshCollection>& InCollection,
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
			InCollection->AppendGeometry(*TetCollection.Get());

			UE_LOG(LogChaosFlesh, Display,
				TEXT("Generated tet mesh via IsoStuffing, num vertices: %d num tets: %d"), Vertices.Num(), Elements.Num());
		}
		else
		{
			UE_LOG(LogChaosFlesh, Warning, TEXT("IsoStuffing produced 0 tetrahedra."));
		}
	}
#else
	ensureMsgf(false, TEXT("FGenerateTetrahedralCollectionDataflowNodes is an editor only node."));
#endif
}

void FGenerateTetrahedralCollectionDataflowNodes::EvaluateTetWild(
	Dataflow::FContext& Context, 
	TUniquePtr<FFleshCollection>& InCollection,
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
			InCollection->AppendGeometry(*TetCollection.Get());

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
	ensureMsgf(false, TEXT("FGenerateTetrahedralCollectionDataflowNodes is an editor only node."));
#endif
}
