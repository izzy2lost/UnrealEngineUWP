// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformationOps/LatticeDeformerOp.h"
#include "DynamicSubmesh3.h"
#include "Operations/FFDLattice.h"

using namespace UE::Geometry;

void FLatticeDeformerOp::CalculateResult(FProgressCancel* Progress)
{
	if (Progress && Progress->Cancelled())
	{
		return;
	}

	ResultMesh->Copy(*OriginalMesh);

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	TArray<FVector3d> DeformedPositions;
	FLatticeExecutionInfo ExecutionInfo = FLatticeExecutionInfo();
	ExecutionInfo.bParallel = true;
	
	const bool bUsingSubmesh = Submesh != nullptr;
	
	// retrieves the deformed vertex positions of the applicable mesh - either the whole mesh or a submesh, depending on selection
	Lattice->GetDeformedMeshVertexPositions(LatticeControlPoints, DeformedPositions, InterpolationType, ExecutionInfo, Progress);
	
	if (bUsingSubmesh)
	{
		check(Submesh->GetSubmesh().VertexCount() == DeformedPositions.Num())
	}
	else
	{
		check(ResultMesh->VertexCount() == DeformedPositions.Num());
	}

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	if (bUsingSubmesh)
	{
		for (const int SubVID : Submesh->GetSubmesh().VertexIndicesItr())
		{
			// retrieve the index in the Base Mesh which maps to the (deformed) submesh
			const int BaseMeshVertexID = Submesh->MapVertexToBaseMesh(SubVID);
			// sets the deformed position of the submesh in the result mesh
			ResultMesh->SetVertex(BaseMeshVertexID, DeformedPositions[SubVID]);
		}
		for (const int BaseVID : ResultMesh->VertexIndicesItr())
		{
			// for all vertices in the base mesh that are NOT in the submesh, they keep their original (transformed) position
			if (Submesh->MapVertexToSubmesh(BaseVID) == INDEX_NONE)
			{
				ResultMesh->SetVertex(BaseVID, WorldTransform.TransformPosition(ResultMesh->GetVertex(BaseVID)));
			}
		}
	}
	else
	{
		// if deforming entire mesh, all the mesh's new positions will be in DeformedPositions
		for (const int VID : ResultMesh->VertexIndicesItr())
		{
			ResultMesh->SetVertex(VID, DeformedPositions[VID]);
		}
	}

	if (bDeformNormals)
	{
		if (ResultMesh->HasAttributes())
		{
			FDynamicMeshNormalOverlay* NormalOverlay = ResultMesh->Attributes()->PrimaryNormals();
			check(NormalOverlay != nullptr);

			TArray<FVector3f> DeformedNormals;
			Lattice->GetRotatedOverlayNormals(LatticeControlPoints,
											  NormalOverlay,
											  DeformedNormals,
											  InterpolationType,
											  ExecutionInfo,
											  Progress);

			if (Progress && Progress->Cancelled())
			{
				return;
			}

			for (int ElementID : NormalOverlay->ElementIndicesItr())
			{
				NormalOverlay->SetElement(ElementID, DeformedNormals[ElementID]);
			}
		}
		else if (ResultMesh->HasVertexNormals())
		{
			TArray<FVector3f> OriginalNormals;
			OriginalNormals.SetNum(ResultMesh->MaxVertexID());
			for (int VertexID : ResultMesh->VertexIndicesItr())
			{
				OriginalNormals[VertexID] = ResultMesh->GetVertexNormal(VertexID);
			}

			TArray<FVector3f> RotatedNormals;
			Lattice->GetRotatedMeshVertexNormals(LatticeControlPoints,
												 OriginalNormals,
												 RotatedNormals,
												 InterpolationType,
												 ExecutionInfo,
												 Progress);

			if (Progress && Progress->Cancelled())
			{
				return;
			}

			for (int vid : ResultMesh->VertexIndicesItr())
			{
				ResultMesh->SetVertexNormal(vid, RotatedNormals[vid]);
			}
		}
	}
}

FLatticeDeformerOp::FLatticeDeformerOp(TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> InOriginalMesh,
									   TSharedPtr<FFFDLattice, ESPMode::ThreadSafe> InLattice,
									   const TArray<FVector3d>& InLatticeControlPoints,
									   ELatticeInterpolation InInterpolationType,
									   bool bInDeformNormals) :
	Lattice(InLattice),
	OriginalMesh(InOriginalMesh),
	LatticeControlPoints(InLatticeControlPoints),
	InterpolationType(InInterpolationType),
	bDeformNormals(bInDeformNormals)
{}

FLatticeDeformerOp::FLatticeDeformerOp(TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> InOriginalMesh,
									   TSharedPtr<FDynamicSubmesh3, ESPMode::ThreadSafe> InSubmesh,
									   FTransform3d InTransform,
									   TSharedPtr<FFFDLattice, ESPMode::ThreadSafe> InLattice,
									   const TArray<FVector3d>& InLatticeControlPoints,
									   ELatticeInterpolation InInterpolationType,
									   bool bInDeformNormals) :
	Lattice(InLattice),
	OriginalMesh(InOriginalMesh),
	Submesh(InSubmesh),
	WorldTransform(InTransform),
	LatticeControlPoints(InLatticeControlPoints),
	InterpolationType(InInterpolationType),
	bDeformNormals(bInDeformNormals)
{}
