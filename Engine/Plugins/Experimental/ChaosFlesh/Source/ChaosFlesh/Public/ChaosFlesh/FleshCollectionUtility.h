// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosFlesh/FleshCollection.h"
#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "Misc/AssertionMacros.h"

class USkeletalMesh;
class UFleshAsset;
class UStaticMesh;

namespace ChaosFlesh 
{
	void GetTetFaces(const FIntVector4& Tet, FIntVector3& Face1, FIntVector3& Face2, FIntVector3& Face3, FIntVector3& Face4, const bool invert = false);

	void GetSurfaceElements(const TArray<FIntVector4>& Tets, TArray<FIntVector3>& SurfaceElements, const bool KeepInteriorFaces, const bool InvertFaces = false);
	
	CHAOSFLESH_API FString GetMeshId(const USkeletalMesh* SkeletalMesh, const bool bUseImportModel);

	CHAOSFLESH_API FString GetMeshId(const UStaticMesh* StaticMesh);

	CHAOSFLESH_API void BoundSurfacePositions(
		const USkeletalMesh* SkeletalMesh,
		const FFleshCollection* FleshCollection,
		const TManagedArray<FVector3f>* RestVertices,
		const TManagedArray<FVector3f>* SimulatedVertices,
		TArray<FVector3f>& Positions);
}
