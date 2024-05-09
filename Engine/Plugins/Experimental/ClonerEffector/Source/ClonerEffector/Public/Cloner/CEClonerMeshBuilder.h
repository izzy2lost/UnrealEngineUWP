// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "CEClonerMeshBuilder.generated.h"

class UBrushComponent;
class UDynamicMesh;
class UDynamicMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class USplineMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Struct used to build a mesh based out of other meshes class */
USTRUCT()
struct FCEClonerMeshBuilder
{
	GENERATED_BODY()

	FCEClonerMeshBuilder();

	/** Resets the builder and clears data */
	void Reset();

	/** Appends supported components within actor */
	int32 AppendActor(const AActor* InActor);

	/** Appends Static Mesh component */
	bool AppendComponent(const UStaticMeshComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends Procedural Mesh component */
	bool AppendComponent(UProceduralMeshComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends Brush component */
	bool AppendComponent(UBrushComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends Skeletal Mesh component */
	bool AppendComponent(const USkeletalMeshComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends Dynamic Mesh component */
	bool AppendComponent(UDynamicMeshComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends Instanced Static Mesh component */
	bool AppendComponent(UInstancedStaticMeshComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends Spline Mesh component */
	bool AppendComponent(USplineMeshComponent* InComponent, const FTransform& InTransform = FTransform::Identity);

	/** Appends a dynamic mesh */
	bool AppendMesh(const UDynamicMesh* InMesh, const TArray<TWeakObjectPtr<UMaterialInterface>>& InMaterials, const FTransform& InTransform = FTransform::Identity);

	/** Builds a dynamic mesh by merging the mesh data imported */
	bool BuildDynamicMesh(UDynamicMesh* OutMesh, TArray<TWeakObjectPtr<UMaterialInterface>>& OutMaterials);

	/** Builds a static mesh by merging the mesh data imported */
	bool BuildStaticMesh(UStaticMesh* OutMesh, TArray<TWeakObjectPtr<UMaterialInterface>>& OutMaterials);

private:
	bool AppendPrimitiveComponent(UPrimitiveComponent* InComponent, const FTransform& InTransform);

	TArray<UE::Geometry::FDynamicMesh3> ConvertedMeshes;

	TArray<TWeakObjectPtr<UMaterialInterface>> OutputMeshMaterials;

	UPROPERTY()
	TObjectPtr<UDynamicMesh> OutputDynamicMesh;
};
