// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "HAL/Platform.h"

#include "Containers/BitArray.h"

namespace mu
{
	class Mesh;

	/** Remove a list of vertices and related faces from a mesh. The list of vertices is stored in a specially formattes Mask mesh. */
	extern void MeshRemoveMask(Mesh* Result, const Mesh* Source, const Mesh* Mask, bool& bOutSuccess);

	/** Remove a list of vertices and related faces from a mesh. The list is stored as a bool map for every vertex in the mesh. */
	//extern void MeshRemoveVerticesWithMap(Mesh* Result, const TBitArray<>& RemovedVertices);

	/** 
	 * Remove a set of vertices and related faces from a mesh in-place. VertexToCull is a bitset where if bit i-th is set, 
	 * the vertex i-th will be removed if all faces referencing this vertex need to be removed. A face is remove if all its 
	 * vertices have the bit set in VerticesToCull.
	 */
	extern void MeshRemoveVerticesWithCullSet(Mesh* Result, const TBitArray<>& VerticesToCull);

	/**
	 * Recreates the Surface and Surfaces Submeshes given a set of vertices and faces remaining after mesh removal.
	 */
	extern void MeshRemoveRecreateSurface(Mesh* Result, const TBitArray<>& UsedVertices, const TBitArray<>& UsedFaces);
}
