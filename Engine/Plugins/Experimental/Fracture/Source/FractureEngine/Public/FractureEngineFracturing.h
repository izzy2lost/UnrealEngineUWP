// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"

#include "FractureEngineFracturing.generated.h"

struct FDataflowTransformSelection;
struct FManagedArrayCollection;

UENUM(BlueprintType)
enum class EFractureBrickBondEnum : uint8
{
	Dataflow_FractureBrickBond_Stretcher UMETA(DisplayName = "Stretcher"),
	Dataflow_FractureBrickBond_Stack UMETA(DisplayName = "Stack"),
	Dataflow_FractureBrickBond_English UMETA(DisplayName = "English"),
	Dataflow_FractureBrickBond_Header UMETA(DisplayName = "Header"),
	Dataflow_FractureBrickBond_Flemish UMETA(DisplayName = "Flemish"),

	//~~~
	//256th entry
	Dataflow_Max                UMETA(Hidden)
};

class FRACTUREENGINE_API FFractureEngineFracturing
{
public:

	static void GenerateExplodedViewAttribute(FManagedArrayCollection& InOutCollection, const FVector& InScale, float InUniformScale);

	static int32 VoronoiFracture(FManagedArrayCollection& InOutCollection,
		FDataflowTransformSelection InTransformSelection,
		TArray<FVector> InSites,
		const FTransform& InTransform,
		int32 InRandomSeed,
		float InChanceToFracture,
		bool InSplitIslands,
		float InGrout,
		float InAmplitude,
		float InFrequency,
		float InPersistence,
		float InLacunarity,
		int32 InOctaveNumber,
		float InPointSpacing,
		bool InAddSamplesForCollision,
		float InCollisionSampleSpacing);

	static int32 PlaneCutter(FManagedArrayCollection& InOutCollection,
		FDataflowTransformSelection InTransformSelection,
		const FBox& InBoundingBox,
		const FTransform& InTransform,
		int32 InNumPlanes,
		int32 InRandomSeed,
		float InChanceToFracture,
		bool InSplitIslands,
		float InGrout,
		float InAmplitude,
		float InFrequency,
		float InPersistence,
		float InLacunarity,
		int32 InOctaveNumber,
		float InPointSpacing,
		bool InAddSamplesForCollision,
		float InCollisionSampleSpacing);

	static void GenerateSliceTransforms(TArray<FTransform>& InOutCuttingPlaneTransforms,
		const FBox& InBoundingBox,
		int32 InSlicesX,
		int32 InSlicesY,
		int32 InSlicesZ,
		int32 InRandomSeed,
		float InSliceAngleVariation,
		float InSliceOffsetVariation);

	static int32 SliceCutter(FManagedArrayCollection& InOutCollection,
		FDataflowTransformSelection InTransformSelection,
		const FBox& InBoundingBox,
		int32 InSlicesX,
		int32 InSlicesY,
		int32 InSlicesZ,
		float InSliceAngleVariation,
		float InSliceOffsetVariation,
		int32 InRandomSeed,
		float InChanceToFracture,
		bool InSplitIslands,
		float InGrout,
		float InAmplitude,
		float InFrequency,
		float InPersistence,
		float InLacunarity,
		int32 InOctaveNumber,
		float InPointSpacing,
		bool InAddSamplesForCollision,
		float InCollisionSampleSpacing);

	static void AddBoxEdges(TArray<TTuple<FVector, FVector>>& InOutEdges, 
		const FVector& InMin, 
		const FVector& InMax);

	static void GenerateBrickTransforms(const FBox& InBounds,
		TArray<FTransform>& InOutBrickTransforms,
		const EFractureBrickBondEnum InBond,
		const float InBrickLength,
		const float InBrickHeight,
		const float InBrickDepth,
		TArray<TTuple<FVector, FVector>>& InOutEdges);

	static int32 BrickCutter(FManagedArrayCollection& InOutCollection,
		FDataflowTransformSelection InTransformSelection,
		const FBox& InBoundingBox, 
		const FTransform& InTransform,
		EFractureBrickBondEnum InBond,
		float InBrickLength,
		float InBrickHeight,
		float InBrickDepth,
		int32 InRandomSeed,
		float InChanceToFracture,
		bool InSplitIslands,
		float InGrout,
		float InAmplitude,
		float InFrequency,
		float InPersistence,
		float InLacunarity,
		int32 InOctaveNumber,
		float InPointSpacing,
		bool InAddSamplesForCollision,
		float InCollisionSampleSpacing);

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Algo/Count.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowSelection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#endif
