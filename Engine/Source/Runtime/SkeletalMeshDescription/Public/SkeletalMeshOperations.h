// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneIndices.h"
#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "MeshTypes.h"
#include "StaticMeshOperations.h"

struct FMeshDescription;


DECLARE_LOG_CATEGORY_EXTERN(LogSkeletalMeshOperations, Log, All);



class FSkeletalMeshOperations : public FStaticMeshOperations
{
public:
	struct FSkeletalMeshAppendSettings
	{
		FSkeletalMeshAppendSettings()
			: SourceVertexIDOffset(0)
		{}

		int32 SourceVertexIDOffset;
		TArray<FBoneIndexType> SourceRemapBoneIndex;
		bool bAppendVertexAttributes = false;
	};
	
	static SKELETALMESHDESCRIPTION_API void AppendSkinWeight(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, FSkeletalMeshAppendSettings& AppendSettings);

	/* Copies skin weight attribute from one mesh to another. Assumes the two geometries are identical or near-identical.
	 * Uses closest triangle on the target mesh to interpolate skin weights to each of the points on the target mesh.
	 * Attributes for the given profiles on both meshes should exist in order for this function to succeed. 
	 * \param InSourceMesh The mesh to copy skin weights from.
	 * \param InTargetMesh The mesh to copy skin weights to.
	 * \param InSourceProfile The name of the skin weight profile on the source mesh to read from.
	 * \param InTargetProfile The name of the skin weight profile on the target mesh to write to.
	 * \param SourceBoneIndexToTargetBoneIndexMap An optional mapping table to map bone indexes on the source mesh to the target mesh.
	 *     The table needs to be complete for all the source bone indexes to valid target bone indexes, otherwise the behavior
	 *     is undefined. If the table is not given, the bone indexes on the source and target meshes are assumed to be the same.
	 */
	static SKELETALMESHDESCRIPTION_API bool CopySkinWeightAttributeFromMesh(
		const FMeshDescription& InSourceMesh,
		FMeshDescription& InTargetMesh,
		const FName InSourceProfile,
		const FName InTargetProfile,
		const TMap<int32, int32>* SourceBoneIndexToTargetBoneIndexMap
		);

	/** Remaps the bone indices on all skin weight attributes from one index to another. The array view should contain a full mapping of all the
	 *  bone indices contained in the skin weights. The array is indexed by the current bone index and the value at that index is the new bone index.
	 *  If the mapping is incomplete or if two entries map to the same bone, the result is undefined. No prior checking is performed.
	 *  \param InMesh The mesh on which to modify all skin weight attributes to remap their bone indices.
	 *  \param InBoneIndexMapping The mapping from one bone index to another. The old bone index is used to index into the array, the value at that
	 *     position is the new bone index.
	 *  \return true if the operation was successful. If the mapping array was incomplete then this will return false. If there are no skin weight
	 *     attributes on the mesh, then the operation is still deemed successful.
	 */  
	static SKELETALMESHDESCRIPTION_API bool RemapBoneIndicesOnSkinWeightAttribute(
		FMeshDescription& InMesh,
		TConstArrayView<int32> InBoneIndexMapping
		);
};
