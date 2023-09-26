// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/CollisionFunctions.h"

#include "Async/ParallelFor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/MeshNormals.h"
#include "Operations/MeshConvexHull.h"
#include "Operations/MeshProjectionHull.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "Spatial/FastWinding.h"
#include "ProjectionTargets.h"
#include "MeshSimplification.h"

#include "Selections/MeshConnectedComponents.h"
#include "DynamicSubmesh3.h"
#include "Polygroups/PolygroupUtil.h"

#include "ShapeApproximation/ShapeDetection3.h"
#include "ShapeApproximation/MeshSimpleShapeApproximation.h"

#include "CompGeom/ConvexDecomposition3.h"
#include "OrientedBoxTypes.h"
#include "MeshQueries.h"
#include "MeshAdapter.h"

#include "Generators/MeshShapeGenerator.h"
#include "Generators/GridBoxMeshGenerator.h"
#include "Generators/BoxSphereGenerator.h"
#include "Generators/CapsuleGenerator.h"

// physics data
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

// requires ModelingComponents
#include "Physics/PhysicsDataCollection.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CollisionFunctions)

#if WITH_EDITOR
#include "Editor.h"
#endif

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_CollisionFunctions"


namespace UELocal
{

void ComputeCollisionFromMesh(
	const FDynamicMesh3& Mesh, 
	FKAggregateGeom& GeneratedCollision, 
	FGeometryScriptCollisionFromMeshOptions& Options)
{
	FPhysicsDataCollection NewCollision;

	FMeshConnectedComponents Components(&Mesh);
	Components.FindConnectedTriangles();
	int32 NumComponents = Components.Num();

	TArray<FDynamicMesh3> Submeshes;
	TArray<const FDynamicMesh3*> SubmeshPointers;

	if (NumComponents == 1)
	{
		SubmeshPointers.Add(&Mesh);
	}
	else
	{
		Submeshes.SetNum(NumComponents);
		SubmeshPointers.SetNum(NumComponents);
		ParallelFor(NumComponents, [&](int32 k)
		{
			FDynamicSubmesh3 Submesh(&Mesh, Components[k].Indices, (int32)EMeshComponents::None, false);
			Submeshes[k] = MoveTemp(Submesh.GetSubmesh());
			SubmeshPointers[k] = &Submeshes[k];
		});
	}

	FMeshSimpleShapeApproximation ShapeGenerator;
	ShapeGenerator.InitializeSourceMeshes(SubmeshPointers);

	ShapeGenerator.bDetectSpheres = Options.bAutoDetectSpheres;
	ShapeGenerator.bDetectBoxes = Options.bAutoDetectBoxes;
	ShapeGenerator.bDetectCapsules = Options.bAutoDetectCapsules;

	ShapeGenerator.MinDimension = Options.MinThickness;

	switch (Options.Method)
	{
	case EGeometryScriptCollisionGenerationMethod::AlignedBoxes:
		ShapeGenerator.Generate_AlignedBoxes(NewCollision.Geometry);
		break;
	case EGeometryScriptCollisionGenerationMethod::OrientedBoxes:
		ShapeGenerator.Generate_OrientedBoxes(NewCollision.Geometry);
		break;
	case EGeometryScriptCollisionGenerationMethod::MinimalSpheres:
		ShapeGenerator.Generate_MinimalSpheres(NewCollision.Geometry);
		break;
	case EGeometryScriptCollisionGenerationMethod::Capsules:
		ShapeGenerator.Generate_Capsules(NewCollision.Geometry);
		break;
	case EGeometryScriptCollisionGenerationMethod::ConvexHulls:
		ShapeGenerator.bSimplifyHulls = Options.bSimplifyHulls;
		ShapeGenerator.HullTargetFaceCount = Options.ConvexHullTargetFaceCount;
		if (Options.MaxConvexHullsPerMesh > 1)
		{
			ShapeGenerator.ConvexDecompositionMaxPieces = Options.MaxConvexHullsPerMesh;
			ShapeGenerator.ConvexDecompositionSearchFactor = Options.ConvexDecompositionSearchFactor;
			ShapeGenerator.ConvexDecompositionErrorTolerance = Options.ConvexDecompositionErrorTolerance;
			ShapeGenerator.ConvexDecompositionMinPartThickness = Options.ConvexDecompositionMinPartThickness;
			ShapeGenerator.Generate_ConvexHullDecompositions(NewCollision.Geometry);
		}
		else
		{
			ShapeGenerator.Generate_ConvexHulls(NewCollision.Geometry);
		}
		break;
	case EGeometryScriptCollisionGenerationMethod::SweptHulls:
		ShapeGenerator.bSimplifyHulls = Options.bSimplifyHulls;
		ShapeGenerator.HullSimplifyTolerance = Options.SweptHullSimplifyTolerance;
		ShapeGenerator.Generate_ProjectedHulls(NewCollision.Geometry, 
			static_cast<FMeshSimpleShapeApproximation::EProjectedHullAxisMode>(Options.SweptHullAxis));
		break;
	case EGeometryScriptCollisionGenerationMethod::MinVolumeShapes:
		ShapeGenerator.Generate_MinVolume(NewCollision.Geometry);
		break;
	}

	if (Options.bRemoveFullyContainedShapes && Components.Num() > 1)
	{
		NewCollision.Geometry.RemoveContainedGeometry();
	}

	if (Options.MaxShapeCount > 0 && Options.MaxShapeCount < Components.Num())
	{
		NewCollision.Geometry.FilterByVolume(Options.MaxShapeCount);
	}

	NewCollision.CopyGeometryToAggregate();
	GeneratedCollision = NewCollision.AggGeom;
}



static void SetStaticMeshSimpleCollision(UStaticMesh* StaticMeshAsset, const FKAggregateGeom& NewSimpleCollision, bool bEmitTransaction)
{
#if WITH_EDITOR
	if (bEmitTransaction && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateStaticMesh", "Set Simple Collision"));

		StaticMeshAsset->Modify();
	}
#endif

	UBodySetup* BodySetup = StaticMeshAsset->GetBodySetup();
	if (BodySetup != nullptr)
	{
		// mark the BodySetup for modification. Do we need to modify the UStaticMesh??
#if WITH_EDITOR
		if (bEmitTransaction)
		{
			BodySetup->Modify();
		}
#endif

		// clear existing simple collision. This will call BodySetup->InvalidatePhysicsData()
		BodySetup->RemoveSimpleCollision();

		// set new collision geometry
		BodySetup->AggGeom = NewSimpleCollision;

		// update collision type
		//BodySetup->CollisionTraceFlag = (ECollisionTraceFlag)(int32)Settings->SetCollisionType;

		// rebuild physics meshes
		BodySetup->CreatePhysicsMeshes();

		StaticMeshAsset->RecreateNavCollision();

		// update physics state on all components using this StaticMesh
		for (FThreadSafeObjectIterator Iter(UStaticMeshComponent::StaticClass()); Iter; ++Iter)
		{
			UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(*Iter);
			if (SMComponent->GetStaticMesh() == StaticMeshAsset)
			{
				if (SMComponent->IsPhysicsStateCreated())
				{
					SMComponent->RecreatePhysicsState();
				}
			}
		}

		// do we need to do a post edit change here??

		// mark static mesh as dirty so it gets resaved?
		StaticMeshAsset->MarkPackageDirty();

#if WITH_EDITORONLY_DATA
		// mark the static mesh as having customized collision so it is not regenerated on reimport
		StaticMeshAsset->bCustomizedCollision = true;
#endif // WITH_EDITORONLY_DATA
	}

#if WITH_EDITOR
	if (bEmitTransaction && GEditor)
	{
		GEditor->EndTransaction();
	}
#endif

}


// local helper to append a convex elem to a compact dynamic mesh, if it has more than a given number of tris
static bool AppendConvexElemToCompactDynamicMesh(const FKConvexElem& Elem, FDynamicMesh3& Mesh, int32 MinTris = 0)
{
	checkSlow(Mesh.IsCompact());
	if (Elem.IndexData.Num() <= MinTris * 3)
	{
		return false;
	}

	int32 StartV = Mesh.MaxVertexID();
	for (FVector V : Elem.VertexData)
	{
		Mesh.AppendVertex(V);
	}
	for (int32 TriStart = 0; TriStart + 2 < Elem.IndexData.Num(); TriStart += 3)
	{
		// Note: We intentially reverse triangle winding here because FKConvexElem stores triangles with the opposite winding vs Dynamic Mesh
		Mesh.AppendTriangle(StartV + Elem.IndexData[TriStart], StartV + Elem.IndexData[TriStart + 2], StartV + Elem.IndexData[TriStart + 1]);
	}

	return true;
}


}		// end namespace UELocal


UDynamicMesh* UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromMesh(
	UDynamicMesh* FromDynamicMesh,
	UStaticMesh* ToStaticMeshAsset,
	FGeometryScriptCollisionFromMeshOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetStaticMeshCollisionFromMesh_InvalidInput1", "SetStaticMeshCollisionFromMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}
	if (ToStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetStaticMeshCollisionFromMesh_InvalidInput2", "SetStaticMeshCollisionFromMesh: ToStaticMeshAsset is Null"));
		return FromDynamicMesh;
	}

	FKAggregateGeom NewCollision;
	FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		UELocal::ComputeCollisionFromMesh(ReadMesh, NewCollision, Options);
	});

	UELocal::SetStaticMeshSimpleCollision(ToStaticMeshAsset, NewCollision, Options.bEmitTransaction);

	return FromDynamicMesh;
}




void UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromComponent(
	UStaticMesh* UpdateStaticMeshAsset, 
	UPrimitiveComponent* SourceComponent,
	FGeometryScriptSetSimpleCollisionOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (UpdateStaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetStaticMeshCollisionFromComponent_InvalidStaticMesh", "SetStaticMeshCollisionFromComponent: UpdateStaticMeshAsset is Null"));
		return;
	}
	if (SourceComponent == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetStaticMeshCollisionFromComponent_InvalidSourceComponent", "SetStaticMeshCollisionFromComponent: SourceComponent is Null"));
		return;
	}

	const UBodySetup* BodySetup = SourceComponent->GetBodySetup();
	if (BodySetup == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetStaticMeshCollisionFromComponent_InvalidBodySetup", "SetStaticMeshCollisionFromComponent: SourceComponent BodySetup is Null"));
		return;
	}

	UELocal::SetStaticMeshSimpleCollision(UpdateStaticMeshAsset, BodySetup->AggGeom, Options.bEmitTransaction);
}





UDynamicMesh* UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
	UDynamicMesh* FromDynamicMesh,
	UDynamicMeshComponent* DynamicMeshComponent,
	FGeometryScriptCollisionFromMeshOptions Options,
	UGeometryScriptDebug* Debug)
{
	if (FromDynamicMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetDynamicMeshCollisionFromMesh_InvalidInput1", "SetDynamicMeshCollisionFromMesh: FromDynamicMesh is Null"));
		return FromDynamicMesh;
	}
	if (DynamicMeshComponent == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SetDynamicMeshCollisionFromMesh_InvalidInput2", "SetDynamicMeshCollisionFromMesh: ToDynamicMeshComponent is Null"));
		return FromDynamicMesh;
	}

	FKAggregateGeom NewCollision;
	FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		UELocal::ComputeCollisionFromMesh(ReadMesh, NewCollision, Options);
	});

#if WITH_EDITOR
	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateDynamicMesh", "Set Simple Collision"));

		DynamicMeshComponent->Modify();
	}
#endif

#if WITH_EDITOR
	if (Options.bEmitTransaction)
	{
		UBodySetup* BodySetup = DynamicMeshComponent->GetBodySetup();
		if (BodySetup != nullptr)
		{
			BodySetup->Modify();
		}
	}
#endif

	// set new collision geometry
	DynamicMeshComponent->SetSimpleCollisionShapes(NewCollision, true /*bUpdateCollision*/);

	// do we need to do a post edit change here??

#if WITH_EDITOR
	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->EndTransaction();
	}
#endif

	return FromDynamicMesh;
}


void UGeometryScriptLibrary_CollisionFunctions::ResetDynamicMeshCollision(
	UDynamicMeshComponent* DynamicMeshComponent,
	bool bEmitTransaction,
	UGeometryScriptDebug* Debug)
{
	if (DynamicMeshComponent == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("ResetDynamicMeshCollision_InvalidInput2", "ResetDynamicMeshCollision: Component is Null"));
		return;
	}

#if WITH_EDITOR
	if (bEmitTransaction && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("ResetDynamicMeshCollisionTransaction", "Clear Simple Collision"));
		DynamicMeshComponent->Modify();
	}
#endif

#if WITH_EDITOR
	if (bEmitTransaction)
	{
		// mark the BodySetup for modification.
		UBodySetup* BodySetup = DynamicMeshComponent->GetBodySetup();
		if (BodySetup != nullptr)
		{
			BodySetup->Modify();
		}
	}
#endif

	// clear existing simple collision.
	DynamicMeshComponent->ClearSimpleCollisionShapes(true /*bUpdateCollision*/);

	// do we need to do a post edit change here??

#if WITH_EDITOR
	if (bEmitTransaction && GEditor)
	{
		GEditor->EndTransaction();
	}
#endif

}


FGeometryScriptSimpleCollision
UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionFromComponent(
	UPrimitiveComponent* Component,
	UGeometryScriptDebug* Debug)
{
	FGeometryScriptSimpleCollision ToRet;
	if (Component == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSimpleCollisionFromComponent_InvalidComponent", "GetSimpleCollisionFromComponent: Component is Null"));
		return ToRet;
	}
	const UBodySetup* BodySetup = Component->GetBodySetup();
	if (BodySetup == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSimpleCollisionFromComponent_InvalidBodySetup", "GetSimpleCollisionFromComponent: Component's BodySetup is Null"));
		return ToRet;
	}
	ToRet.AggGeom = BodySetup->AggGeom;
	
	return ToRet;
}

void UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(
	const FGeometryScriptSimpleCollision& SimpleCollision,
	UDynamicMeshComponent* DynamicMeshComponent,
	FGeometryScriptSetSimpleCollisionOptions Options,
	UGeometryScriptDebug* Debug)
{
#if WITH_EDITOR
	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->BeginTransaction(LOCTEXT("UpdateDynamicMesh", "Set Simple Collision"));

		DynamicMeshComponent->Modify();
	}
#endif

#if WITH_EDITOR
	if (Options.bEmitTransaction)
	{
		UBodySetup* BodySetup = DynamicMeshComponent->GetBodySetup();
		if (BodySetup != nullptr)
		{
			BodySetup->Modify();
		}
	}
#endif

	// set new collision geometry
	DynamicMeshComponent->SetSimpleCollisionShapes(SimpleCollision.AggGeom, true /*bUpdateCollision*/);

	// do we need to do a post edit change here??

#if WITH_EDITOR
	if (Options.bEmitTransaction && GEditor)
	{
		GEditor->EndTransaction();
	}
#endif
}


FGeometryScriptSimpleCollision UGeometryScriptLibrary_CollisionFunctions::GetSimpleCollisionFromStaticMesh(
	UStaticMesh* StaticMeshAsset, UGeometryScriptDebug* Debug)
{
	FGeometryScriptSimpleCollision ToRet;
	
	if (StaticMeshAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSimpleCollisionFromStaticMesh_InvalidStaticMesh", "GetSimpleCollisionFromStaticMesh: Input Mesh is Null"));
		return ToRet;
	}
	const UBodySetup* BodySetup = StaticMeshAsset->GetBodySetup();
	if (BodySetup == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("GetSimpleCollisionFromStaticMesh_InvalidBodySetup", "GetSimpleCollisionFromStaticMesh: Input Mesh's BodySetup is Null"));
		return ToRet;
	}

	ToRet.AggGeom = BodySetup->AggGeom;

	return ToRet;
}

void UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfStaticMesh(
	const FGeometryScriptSimpleCollision& SimpleCollision,
	UStaticMesh* StaticMesh, 
	FGeometryScriptSetSimpleCollisionOptions Options,
	UGeometryScriptDebug* Debug)
{
	UELocal::SetStaticMeshSimpleCollision(StaticMesh, SimpleCollision.AggGeom, Options.bEmitTransaction);
}

void UGeometryScriptLibrary_CollisionFunctions::SimplifyConvexHulls(
	FGeometryScriptSimpleCollision& SimpleCollision,
	const FGeometryScriptConvexHullSimplificationOptions& SimplifyOptions,
	bool& bHasSimplified,
	UGeometryScriptDebug* Debug
)
{
	bHasSimplified = false;
	TArray<FKConvexElem>& ConvexElems = SimpleCollision.AggGeom.ConvexElems;
	for (int32 ConvexIdx = 0; ConvexIdx < ConvexElems.Num(); ++ConvexIdx)
	{
		FKConvexElem& Elem = ConvexElems[ConvexIdx];
		Elem.ComputeChaosConvexIndices(false); // make sure indices are computed

		int32 TargetTriangleCount = FMath::Max(4, SimplifyOptions.MinTargetFaceCount);

		// Convert hull to a dynamic mesh
		FDynamicMesh3 Mesh;
		if (!UELocal::AppendConvexElemToCompactDynamicMesh(Elem, Mesh, TargetTriangleCount))
		{
			continue;
		}

		int32 InitialTriangleCount = Mesh.TriangleCount();

		// Run simplification
		FVolPresMeshSimplification Simplifier(&Mesh);
		Simplifier.CollapseMode = FVolPresMeshSimplification::ESimplificationCollapseModes::MinimalExistingVertexError;
		Simplifier.GeometricErrorConstraint = UE::Geometry::FVolPresMeshSimplification::EGeometricErrorCriteria::PredictedPointToProjectionTarget;
		Simplifier.GeometricErrorTolerance = SimplifyOptions.SimplificationDistanceThreshold;

		FDynamicMesh3 ProjectionTargetMesh(Mesh);
		FDynamicMeshAABBTree3 ProjectionTargetSpatial(&ProjectionTargetMesh, true);
		FMeshProjectionTarget ProjTarget(&ProjectionTargetMesh, &ProjectionTargetSpatial);
		Simplifier.SetProjectionTarget(&ProjTarget);
		Simplifier.SimplifyToTriangleCount(TargetTriangleCount);

		// Simplification didn't reduce triangle count, so skip updating the convex hull
		if (Mesh.TriangleCount() == InitialTriangleCount)
		{
			continue;
		}

		Elem.VertexData.Reset(Mesh.VertexCount());
		for (FVector3d V : Mesh.VerticesItr())
		{
			Elem.VertexData.Add(V);
		}
		Elem.UpdateElemBox();
		bHasSimplified = true;
	}
}

FGeometryScriptSimpleCollision UGeometryScriptLibrary_CollisionFunctions::MergeSimpleCollisionShapes(
	const FGeometryScriptSimpleCollision& SimpleCollision,
	const FGeometryScriptMergeSimpleCollisionOptions& MergeOptions,
	bool& bHasMerged,
	UGeometryScriptDebug* Debug
)
{
	FGeometryScriptSimpleCollision ToRet;
	bHasMerged = false;
	
	// Nothing to merge
	if (SimpleCollision.AggGeom.GetElementCount() <= 1)
	{
		return SimpleCollision;
	}

	TArray<FVector> HullVertices;
	TArray<int32> HullVertexCounts;
	TArray<double> HullVolumes;
	TArray<const FKShapeElem*> HullToShapeElem;

	auto TransformVertices = [](TArrayView<FVector3d> Vertices, const FTransform& Transform)
	{
		for (FVector3d& Vertex : Vertices)
		{
			Vertex = Transform.TransformPosition(Vertex);
		}
	};
	auto AppendHullVertices = [&HullToShapeElem, &HullVolumes, &HullVertices, &HullVertexCounts]
				(TArrayView<const FVector3d> Vertices, double Volume, const FKShapeElem* ShapeElem)
	{
		check(HullToShapeElem.Num() == HullVolumes.Num());
		HullToShapeElem.Add(ShapeElem);
		HullVertices.Append(Vertices);
		HullVertexCounts.Add(Vertices.Num());
		HullVolumes.Add(Volume);
	};
	auto GeneratorVolume = [](FMeshShapeGenerator* Generator) -> double
	{
		TIndexVectorMeshArrayAdapter<FIndex3i, double, FVector3d> GenMeshAdapter(&Generator->Vertices, &Generator->Triangles);
		FVector2d VolArea = TMeshQueries<TIndexVectorMeshArrayAdapter<FIndex3i, double, FVector3d>>::GetVolumeArea(GenMeshAdapter);
		return VolArea.X;
	};

	for (const FKBoxElem& Box : SimpleCollision.AggGeom.BoxElems)
	{
		FOrientedBox3d OrientedBox;
		OrientedBox.Extents = FVector(Box.X * .5, Box.Y * .5, Box.Z * .5);
		OrientedBox.Frame.Origin = Box.Center;
		OrientedBox.Frame.Rotation = (FQuaterniond)Box.Rotation;
		TArray<FVector3d, TFixedAllocator<8>> BoxVertices;
		OrientedBox.EnumerateCorners([&](FVector3d Corner) { BoxVertices.Add(Corner); });
		AppendHullVertices(BoxVertices, Box.GetScaledVolume(FVector3d::One()), &Box);
	}
	for (const FKSphereElem& Sphere : SimpleCollision.AggGeom.SphereElems)
	{
		FBoxSphereGenerator SphereGenerator;
		SphereGenerator.Box.Frame.Origin = Sphere.Center;
		SphereGenerator.Radius = FMath::Max(FMathf::ZeroTolerance, Sphere.Radius);
		int32 StepsPerSide = FMath::Max(1, MergeOptions.ShapeToHullTriangulation.SphereStepsPerSide);
		SphereGenerator.EdgeVertices = FIndex3i(StepsPerSide, StepsPerSide, StepsPerSide);
		SphereGenerator.bPolygroupPerQuad = false;
		SphereGenerator.Generate();
		double Volume = GeneratorVolume(&SphereGenerator);
		AppendHullVertices(SphereGenerator.Vertices, Volume, &Sphere);
	}
	for (const FKSphylElem& Capsule : SimpleCollision.AggGeom.SphylElems)
	{
		FCapsuleGenerator CapsuleGenerator;
		CapsuleGenerator.Radius = Capsule.Radius;
		CapsuleGenerator.SegmentLength = Capsule.Length;
		CapsuleGenerator.NumHemisphereArcSteps = FMath::Max(2, MergeOptions.ShapeToHullTriangulation.CapsuleHemisphereSteps);
		CapsuleGenerator.NumCircleSteps = FMath::Max(3, MergeOptions.ShapeToHullTriangulation.CapsuleCircleSteps);
		CapsuleGenerator.bPolygroupPerQuad = false;
		CapsuleGenerator.Generate();
		FTransform CapsuleTransform(Capsule.Rotation, Capsule.Center);
		TransformVertices(CapsuleGenerator.Vertices, CapsuleTransform);
		double Volume = GeneratorVolume(&CapsuleGenerator);
		AppendHullVertices(CapsuleGenerator.Vertices, Volume, &Capsule);
	}
	for (const FKConvexElem& Convex : SimpleCollision.AggGeom.ConvexElems)
	{
		// Note: Not reliable to use the FKConvexElem::GetVolume function because it depends on the chaos convex being allocated, and also is not currently exported
		TIndexMeshArrayAdapter<int32, double, FVector3d> HullMeshAdapter(&Convex.VertexData, &Convex.IndexData);
		// Note: We take the negative volume because the hull triangles have opposite winding from ordinary meshes
		double Volume = -TMeshQueries<TIndexMeshArrayAdapter<int32, double, FVector3d>>::GetVolumeArea(HullMeshAdapter).X;
		AppendHullVertices(Convex.VertexData, Volume, &Convex);
	}

	const int32 InitialNumConvex = HullVertexCounts.Num();
	// Nothing we are able to merge
	if (InitialNumConvex <= 1)
	{
		return SimpleCollision;
	}

	TArray<int32> HullVertexStarts;
	HullVertexStarts.SetNumUninitialized(InitialNumConvex);
	HullVertexStarts[0] = 0; // Note InitialNumConvex is > 1 due to above test
	for (int32 HullIdx = 1, LastEnd = HullVertexCounts[0]; HullIdx < InitialNumConvex; LastEnd += HullVertexCounts[HullIdx++])
	{
		HullVertexStarts[HullIdx] = LastEnd;
	}

	// Currently we use dense proximity to propose which shapes can be merged.
	// Note: To efficiently handle larger shape counts, consider optionally limiting these by some approximate proximity (e.g. expanded bounding box overlap)
	TArray<TPair<int32, int32>> HullProximity;
	for (int32 ConvexA = 0; ConvexA < InitialNumConvex; ++ConvexA)
	{
		for (int32 ConvexB = ConvexA + 1; ConvexB < InitialNumConvex; ++ConvexB)
		{
			HullProximity.Emplace(ConvexA, ConvexB);
		}
	}

	FConvexDecomposition3 Decomposition;
	Decomposition.InitializeFromHulls(HullVertexStarts.Num(),
		[&HullVolumes](int32 HullIdx) { return HullVolumes[HullIdx]; }, [&HullVertexCounts](int32 HullIdx) { return HullVertexCounts[HullIdx]; },
		[&HullVertexStarts, &HullVertices](int32 HullIdx, int32 VertIdx) { return HullVertices[HullVertexStarts[HullIdx] + VertIdx]; }, HullProximity);
	Decomposition.MergeBest(MergeOptions.MaxShapeCount, MergeOptions.ErrorTolerance, 0.0, true, false, MergeOptions.MaxShapeCount, nullptr /*optional negative space*/, nullptr /*optional FTransform for negative space*/);

	// Algorithm decided not to merge
	if (Decomposition.NumHulls() == InitialNumConvex)
	{
		return SimpleCollision;
	}

	bHasMerged = true;

	// Merging logic for the below primitives is not implemented, so they are simply copied over for now
	if (!SimpleCollision.AggGeom.TaperedCapsuleElems.IsEmpty() || !SimpleCollision.AggGeom.SkinnedLevelSetElems.IsEmpty() || !SimpleCollision.AggGeom.LevelSetElems.IsEmpty())
	{
		UE::Geometry::AppendWarning(Debug, EGeometryScriptErrorType::OperationFailed, LOCTEXT("PrimitiveFunctions_AppendSimpleCollisionShapes Unsupported Shapes", "MergeSimpleCollisionShapes: Merging for Tapered Capsules and Level Set collision is not yet supported; these shapes will be copied without considering them for merging."));
		for (const FKTaperedCapsuleElem& Capsule : SimpleCollision.AggGeom.TaperedCapsuleElems)
		{
			ToRet.AggGeom.TaperedCapsuleElems.Add(Capsule);
		}
		for (const FKSkinnedLevelSetElem& LevelSet : SimpleCollision.AggGeom.SkinnedLevelSetElems)
		{
			ToRet.AggGeom.SkinnedLevelSetElems.Add(LevelSet);
		}
		for (const FKLevelSetElem& LevelSet : SimpleCollision.AggGeom.LevelSetElems)
		{
			ToRet.AggGeom.LevelSetElems.Add(LevelSet);
		}
	}
	
	for (int32 HullIdx = 0; HullIdx < Decomposition.Decomposition.Num(); ++HullIdx)
	{
		const FConvexDecomposition3::FConvexPart& Part = Decomposition.Decomposition[HullIdx];
		// If part was not merged, use the source ID to map it back to the original collision primitive
		if (Part.HullSourceID >= 0)
		{
			const FKShapeElem* Elem = HullToShapeElem[Part.HullSourceID];
			bool bHandledShape = true;
			switch (Elem->GetShapeType())
			{
			case EAggCollisionShape::Box:
				ToRet.AggGeom.BoxElems.Add(*static_cast<const FKBoxElem*>(Elem));
				break;
			case EAggCollisionShape::Sphere:
				ToRet.AggGeom.SphereElems.Add(*static_cast<const FKSphereElem*>(Elem));
				break;
			case EAggCollisionShape::Sphyl:
				ToRet.AggGeom.SphylElems.Add(*static_cast<const FKSphylElem*>(Elem));
				break;
			case EAggCollisionShape::Convex:
				ToRet.AggGeom.ConvexElems.Add(*static_cast<const FKConvexElem*>(Elem));
				break;
			default:
				// Note: All shapes that we add to the HullToShapeElem array should be handled above, so we should not reach here
				ensureMsgf(false, TEXT("Unhandled shape element type could not be restored from source shapes"));
				bHandledShape = false;
			}
			if (bHandledShape)
			{
				continue;
			}
		}
		// Add the merged part
		FKConvexElem& Convex = ToRet.AggGeom.ConvexElems.Emplace_GetRef();
		Convex.VertexData = Decomposition.GetVertices<double>(HullIdx);
		Convex.UpdateElemBox(); // Note: In addition to updating the bounding box, this also re-computes hull indices.
	}
	
	return ToRet;
}

#undef LOCTEXT_NAMESPACE
