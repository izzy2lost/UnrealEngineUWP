// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshCreateTetrahedronNode.h"

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
		TUniquePtr<FFleshCollection> InCollection(GetValue<DataType>(Context, &Collection).NewCopy<FFleshCollection>());
		TUniquePtr<FFleshCollection> InSourceCollection(GetValue<DataType>(Context, &SourceCollection).NewCopy<FFleshCollection>());

		// TransformGroup
		int32 NumTransforms = InSourceCollection->NumElements(FTransformCollection::TransformGroup);
		TManagedArray<FString>* TransformName = InSourceCollection->FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
		TManagedArray<int32>* TransformToGeometryIndex = InSourceCollection->FindAttribute<int32>("TransformToGeometryIndex", FTransformCollection::TransformGroup);
		const TManagedArray<int32>* Parent = InSourceCollection->FindAttribute<int32>(FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup);
		const TManagedArray<FTransform3f>* LocalSpaceTransform = InSourceCollection->FindAttribute<FTransform3f>(FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup);
		// Geometry Group
		int32 NumGeometry = InSourceCollection->NumElements(FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* GroupToTransformIndex = InSourceCollection->FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* VertexCount = InSourceCollection->FindAttribute<int32>("VertexCount", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* VertexStart = InSourceCollection->FindAttribute<int32>("VertexStart", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* FaceCount = InSourceCollection->FindAttribute<int32>("FaceCount", FGeometryCollection::GeometryGroup);
		TManagedArray<int32>* FaceStart = InSourceCollection->FindAttribute<int32>("FaceStart", FGeometryCollection::GeometryGroup);

		// Vertices Group
		int32 NumVertices = InSourceCollection->NumElements(FGeometryCollection::VerticesGroup);
		const TManagedArray<FVector3f>* Vertex = InSourceCollection->FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
		// Faces Group
		int32 NumTriangles = InSourceCollection->NumElements(FGeometryCollection::FacesGroup);
		const TManagedArray<FIntVector>* Faces = InSourceCollection->FindAttribute<FIntVector>("Indices", FGeometryCollection::FacesGroup);

		if (TransformName && TransformToGeometryIndex && Parent && LocalSpaceTransform &&
			GroupToTransformIndex && VertexCount && VertexStart && FaceCount && FaceStart
			&& Vertex && Faces)
		{
			auto GetParentIndex = [&InCollection, &InSourceCollection](int32 Gdx)
			{
				if (Gdx >= 0)
				{
					TManagedArray<FString>* SourceTransformName = InSourceCollection->FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
					TManagedArray<FString>* TransformName = InCollection->FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
					TManagedArray<int32>* SourceGroupToTransformIndex = InSourceCollection->FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
					TManagedArray<int32>* GroupToTransformIndex = InCollection->FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
					TManagedArray<int32>* ParentIndex = InCollection->FindAttribute<int32>("Parent", FGeometryCollection::TransformGroup);
					if (SourceTransformName && TransformName && SourceGroupToTransformIndex && GroupToTransformIndex && ParentIndex)
					{
						if (Gdx < SourceGroupToTransformIndex->Num() && Gdx < GroupToTransformIndex->Num())
						{
							int32 SrcTdx = (*SourceGroupToTransformIndex)[Gdx], Tdx = (*GroupToTransformIndex)[Gdx];
							if (0 <= SrcTdx && SrcTdx < SourceTransformName->Num() && 0 <= Tdx && Tdx < TransformName->Num())
							{
								if ((*SourceTransformName)[SrcTdx].Equals((*TransformName)[Tdx]))
								{
									return (*ParentIndex)[Tdx];
								}
							}
						}
					}
				}
				return (int32)INDEX_NONE;
			};

			TArray<int32> ProcessGeometryIndices = Dataflow::GetMatchingMeshIndices(Selection, InSourceCollection.Get());

			TArray<TUniquePtr<FFleshCollection>> CollectionBuffer;
			for (int32 Gdx = 0; Gdx < NumGeometry; Gdx++)
			{
				if (ProcessGeometryIndices.Contains(Gdx))
				{
					CollectionBuffer.Add(TUniquePtr<FFleshCollection>(new FFleshCollection()));
				}
				else 
					CollectionBuffer.Add(nullptr);
			}

			TArray<FTransform> ComponentTransform;
			GeometryCollectionAlgo::GlobalMatrices(*LocalSpaceTransform, *Parent, ComponentTransform);

			ParallelFor(ProcessGeometryIndices.Num(), [&](int32 i)
			{
				int32 Gdx = ProcessGeometryIndices[i];
			    int32 Tdx = (*GroupToTransformIndex)[Gdx];
				FFleshCollection& TetCollection = *CollectionBuffer[Gdx];

				UE::Geometry::FDynamicMesh3 DynamicMesh;
				int32 vStart = (*VertexStart)[Gdx], vEnd = vStart + (*VertexCount)[Gdx];
				for (int Vdx = vStart; Vdx < vEnd; Vdx++) DynamicMesh.AppendVertex(ComponentTransform[Tdx].TransformPosition(FVector((*Vertex)[Vdx])));
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

			auto AppendProcessedGeometry = [&ProcessGeometryIndices, &CollectionBuffer, &InSourceCollection](FFleshCollection& ToCollection)
			{
				// find the index in the to transform group based on name
				auto FindParentIndexFromName = [](FFleshCollection& fpColllection, FString fpName) 
				{
					if (!fpName.IsEmpty())
					{
						const TManagedArray<FString>& Names = fpColllection.GetAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
						for (int32 i = 0; i < Names.Num(); i++) if (Names[i].Equals(fpName)) return i;
					}
					return (int32)INDEX_NONE;
				};

				TManagedArray<int32>* SourceGroupToTransformIndex = InSourceCollection->FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
				TManagedArray<FString>* SourceTransformName = InSourceCollection->FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
				TManagedArray<int32>* SourceParentIndex = InSourceCollection->FindAttribute<int32>("Parent", FTransformCollection::TransformGroup);
				TManagedArray<int32>* ToGroupToTransformIndex = ToCollection.FindAttribute<int32>("TransformIndex", FGeometryCollection::GeometryGroup);
				TManagedArray<int32>* TransformToGroupIndex = ToCollection.FindAttribute<int32>("TransformToGeometryIndex", FTransformCollection::TransformGroup);
				TManagedArray<FString>* ToTransformName = ToCollection.FindAttribute<FString>("BoneName", FTransformCollection::TransformGroup);
				TManagedArray<FTransform3f>* ToTransform = ToCollection.FindAttribute<FTransform3f>("Transform", FTransformCollection::TransformGroup);
				TManagedArray<int32>* ToParentIndex = ToCollection.FindAttribute<int32>("Parent", FTransformCollection::TransformGroup);
				TManagedArray<TSet<int32> >* ToChildIndex = ToCollection.FindAttribute< TSet<int32> >(FTransformCollection::ChildrenAttribute, FTransformCollection::TransformGroup);
				TManagedArray<FVector3f>* ToVertex = ToCollection.FindAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
				TManagedArray<int32>* ToVertexCount = ToCollection.FindAttribute<int32>("VertexCount", FGeometryCollection::GeometryGroup);
				TManagedArray<int32>* ToVertexStart = ToCollection.FindAttribute<int32>("VertexStart", FGeometryCollection::GeometryGroup);

				if (SourceGroupToTransformIndex && SourceTransformName && ToGroupToTransformIndex && ToTransformName && ToParentIndex && SourceParentIndex && ToChildIndex)
				{
					for (int32 Sdx = 0; Sdx < ProcessGeometryIndices.Num(); Sdx++)
					{
						int32 Gdx = ProcessGeometryIndices[Sdx];
						if (CollectionBuffer[Gdx] && CollectionBuffer[Gdx]->NumElements(FGeometryCollection::GeometryGroup))
						{
							int32 GeomIndex = ToCollection.NumElements(FGeometryCollection::GeometryGroup);
							ToCollection.AppendGeometry(*CollectionBuffer[Gdx]);
							// source data
							int32 SourceNumTransforms = InSourceCollection->NumElements(FGeometryCollection::TransformGroup);
							int32 SourceTransformIndex = (*SourceGroupToTransformIndex)[Gdx];
							int32 SourceTransformParent = (0 <= SourceTransformIndex && SourceTransformIndex < SourceNumTransforms) ? (*SourceParentIndex)[SourceTransformIndex] : INDEX_NONE;
							FString SourceParentName = (0 <= SourceTransformParent && SourceTransformParent < SourceNumTransforms) ? (*SourceTransformName)[SourceTransformParent] : FString("");
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
								// set the name
								(*ToTransformName)[ToGeomTransformIndex] = TetName;
				
								// set transform to geometry and geometry to transform mappings
								(*TransformToGroupIndex)[ToGeomTransformIndex] = GeomIndex;
								(*ToGroupToTransformIndex)[GeomIndex] = ToGeomTransformIndex;
								// set the parent and child mappings
								(*ToParentIndex)[ToGeomTransformIndex] = FindParentIndexFromName(ToCollection, SourceParentName);
								if ((*ToParentIndex)[ToGeomTransformIndex] != INDEX_NONE)
								{
									(*ToChildIndex)[(*ToParentIndex)[ToGeomTransformIndex]].Add(ToGeomTransformIndex);
								}

								if (ToGeomTransformIndex != INDEX_NONE)
								{
									int32 VertexEnd = (*ToVertexStart)[GeomIndex] + (*ToVertexCount)[GeomIndex];
									FTransform3f ParentTransform = GeometryCollectionAlgo::GlobalMatrix3f(*ToTransform, *ToParentIndex, ToGeomTransformIndex);
									for (int Vdx = (*ToVertexStart)[GeomIndex]; Vdx < VertexEnd; Vdx++)
									{
										(*ToVertex)[Vdx] = ParentTransform.InverseTransformPosition((*ToVertex)[Vdx]);
									}
								}
							}
						}
					}
				}
			};
			AppendProcessedGeometry(*InCollection);
		}
		SetValue<const DataType&>(Context, *InCollection, &Collection);
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
