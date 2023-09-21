// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "CollisionFunctions.generated.h"

class UDynamicMesh;
class UDynamicMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class EGeometryScriptCollisionGenerationMethod : uint8
{
	AlignedBoxes = 0,
	OrientedBoxes = 1,
	MinimalSpheres = 2,
	Capsules = 3,
	ConvexHulls = 4,
	SweptHulls = 5,
	MinVolumeShapes = 6
};


UENUM(BlueprintType)
enum class EGeometryScriptSweptHullAxis : uint8
{
	X = 0,
	Y = 1,
	Z = 2,
	/** Use X/Y/Z axis with smallest axis-aligned-bounding-box dimension */
	SmallestBoxDimension = 3,
	/** Compute projected hull for each of X/Y/Z axes and use the one that has the smallest volume  */
	SmallestVolume = 4
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptCollisionFromMeshOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEmitTransaction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptCollisionGenerationMethod Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoDetectSpheres = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoDetectBoxes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoDetectCapsules = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float MinThickness = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSimplifyHulls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int ConvexHullTargetFaceCount = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int MaxConvexHullsPerMesh = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float ConvexDecompositionSearchFactor = .5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float ConvexDecompositionErrorTolerance = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float ConvexDecompositionMinPartThickness = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float SweptHullSimplifyTolerance = 0.1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptSweptHullAxis SweptHullAxis = EGeometryScriptSweptHullAxis::Z;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bRemoveFullyContainedShapes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int MaxShapeCount = 0;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSetSimpleCollisionOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEmitTransaction = true;
};

// Options controlling how collision shapes can be merged together
USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptMergeSimpleCollisionOptions
{
	GENERATED_BODY()
public:

	/**
	 * If > 0, merge down to at most this many simple shapes. (If <= 0, this value is ignored.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int MaxShapeCount = 0;

	/**
	 * Error tolerance to use to decide to convex hulls together, in cm.
	 * If merging two hulls would increase the volume by more than this ErrorTolerance cubed, the merge is not accepted.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options, meta = (UIMin = "0", UIMax = "100.", Units = cm))
	double ErrorTolerance = 0.0;

	// Controls for how smooth shapes can be triangulated when/if converted to a convex hull for a merge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ConvexHulls)
	FGeometryScriptSimpleCollisionTriangulationOptions ShapeToHullTriangulation;
};


UCLASS(meta = (ScriptName = "GeometryScript_Collision"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_CollisionFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/** 
	* Generates Simple Collision shapes for a Static Mesh Asset based on the input Dynamic Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	SetStaticMeshCollisionFromMesh(
		UDynamicMesh* FromDynamicMesh, 
		UStaticMesh* ToStaticMeshAsset, 
		FGeometryScriptCollisionFromMeshOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Copy the Simple Collision Geometry from the Source Component to the Static Mesh Asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static void 
	SetStaticMeshCollisionFromComponent(
		UStaticMesh* StaticMeshAsset, 
		UPrimitiveComponent* SourceComponent,
		FGeometryScriptSetSimpleCollisionOptions Options = FGeometryScriptSetSimpleCollisionOptions(),
		UGeometryScriptDebug* Debug = nullptr);

	/** 
	* Generate Simple Collision shapes for a Dynamic Mesh Component based on the input Dynamic Mesh. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	SetDynamicMeshCollisionFromMesh(
		UDynamicMesh* FromDynamicMesh, 
		UDynamicMeshComponent* ToDynamicMeshComponent,
		FGeometryScriptCollisionFromMeshOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

    /** 
	* Clears Simple Collisions from the Dynamic Mesh Component. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static void
	ResetDynamicMeshCollision(
		UDynamicMeshComponent* Component,
		bool bEmitTransaction = false,
		UGeometryScriptDebug* Debug = nullptr);

	/*
	 * Get the simple collision from a Primitive Component
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static UPARAM(DisplayName = "Simple Collision") FGeometryScriptSimpleCollision
	GetSimpleCollisionFromComponent(
		UPrimitiveComponent* Component,
		UGeometryScriptDebug* Debug = nullptr);

	/*
	 * Set the simple collision on a Dynamic Mesh Component
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static void
	SetSimpleCollisionOfDynamicMeshComponent(
		const FGeometryScriptSimpleCollision& SimpleCollision,
		UDynamicMeshComponent* DynamicMeshComponent,
		FGeometryScriptSetSimpleCollisionOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	/*
	 * Get the simple collision from a Static Mesh
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static UPARAM(DisplayName = "Simple Collision") FGeometryScriptSimpleCollision
	GetSimpleCollisionFromStaticMesh(UStaticMesh* StaticMesh, UGeometryScriptDebug* Debug = nullptr);

	/*
	 * Set the simple collision on a Static Mesh
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static void
	SetSimpleCollisionOfStaticMesh(
		const FGeometryScriptSimpleCollision& SimpleCollision,
		UStaticMesh* StaticMesh, 
		FGeometryScriptSetSimpleCollisionOptions Options, 
		UGeometryScriptDebug* Debug = nullptr);

	/*
	 * Count of number of simple collision shapes
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision", meta = (ScriptMethod))
	static int32 GetSimpleCollisionShapeCount(const FGeometryScriptSimpleCollision& SimpleCollision)
	{
		return SimpleCollision.AggGeom.GetElementCount();
	}


	/**
	 * Attempt to merge collision shapes to create a representation with fewer overall shapes.
	 * 
	 * @param SimpleCollision		The collision to attempt to simplify by merging shapes
	 * @param MergeOptions			Options controlling how shapes can be merged
	 * @param bHasMerged			Indicates whether any shapes have been merged
	 * @return						Simple Collision with collision shapes merged, as allowed by settings
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Collision")
	static UPARAM(DisplayName = "Merged Simple Collision") FGeometryScriptSimpleCollision MergeSimpleCollisionShapes(
		const FGeometryScriptSimpleCollision& SimpleCollision,
		const FGeometryScriptMergeSimpleCollisionOptions& MergeOptions,
		bool& bHasMerged,
		UGeometryScriptDebug* Debug = nullptr
	);


};