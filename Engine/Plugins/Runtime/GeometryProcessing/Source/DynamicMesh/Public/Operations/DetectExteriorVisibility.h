// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "TransformSequence.h"

namespace UE::Geometry
{

class FDynamicMesh3;

// Parameters controlling how exterior visiblity is tested
struct FExteriorVisibilitySampling
{
	// Approximate spacing between triangle samples used for visibility tests
	double SamplingDensity = 1.0;

	// Whether to treat faces as double-sided
	bool bDoubleSided = false;

	// Number of directions to test for visibility
	int32 NumSearchDirections = 128;

	// Compute per-triangle visibility array for a triangle mesh
	// @param OutTriOccluded	For each valid triangle ID, OutTriOccluded[ID] will be true if that triangle is hidden, false if it is visible
	// @param SkipTris			True for triangles do not need visibility testing (will be marked as not occluded). If empty, no triangles will be skipped.
	// @param bPerPolyGroup		If true, visiblity will be determined on a per group basis: If any triangle in a group is visible, the whole group will be marked as not-occluded
	// @param bDoubleSided		Whether to treat faces a double-sided
	// @param SamplingDensity	Approximate spacing between triangle samples used for visibility tests
	DYNAMICMESH_API static void ComputePerTriangleOcclusion(const FDynamicMesh3& Mesh, TArray<bool>& OutTriOccluded, bool bPerPolyGroup, const FExteriorVisibilitySampling& SamplingParameters, TArrayView<const bool> SkipTris = TArrayView<const bool>());
};

// Determine which meshes are visible from exterior views of a list of meshes
class FDetectPerDynamicMeshExteriorVisibility
{
public:

	struct FDynamicMeshInstance
	{
		const FDynamicMesh3* SourceMesh;
		FTransformSequence3d Transforms;
		FDynamicMeshInstance() = default;
		FDynamicMeshInstance(FDynamicMesh3* Mesh, const FTransform& InTransform) : SourceMesh(Mesh)
		{
			Transforms.Append(InTransform);
		}
	};

	//
	// Inputs
	//

	// Instances for which we need to determine visibility
	TArray<FDynamicMeshInstance> Instances;
	
	// Instances which occlude visibility, but for which we don't need to determine visibility
	TArray<FDynamicMeshInstance> OccludeInstances;

	// Parameters controlling how the triangles are sampled for visibility
	FExteriorVisibilitySampling SamplingParameters;

	// Compute which source meshes are hidden
	// @param OutIsOccluded		Indicate if each mesh in Instances is hidden (true) or visible (false)
	DYNAMICMESH_API void ComputeHidden(TArray<bool>& OutIsOccluded);

};


} // end namespace UE::Geometry
