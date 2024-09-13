// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "Compute/PCGDataForGPU.h"

#include "ComputeFramework/ComputeGraphInstance.h"

#include "PCGDataBinding.generated.h"

class UPCGComponent;
class UPCGComputeGraph;

USTRUCT()
struct FPCGSpawnerPrimitives
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UPrimitiveComponent>> Primitives;

	/** Cumulative distribution function values (one per primitive) to enable choosing a primitive based on a random draw value. */
	UPROPERTY()
	TArray<float> SelectionCDF;

	// Same for all primitives
	UPROPERTY()
	uint32 NumCustomFloats = 0;

	// Same for all primitives
	UPROPERTY()
	TArray<FUintVector4> AttributeIdOffsetStrides;
};

UCLASS(Transient, Category = PCG)
class UPCGDataBinding : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UPCGComponent> SourceComponent;

	FPCGDataCollection OutputDataCollection;

	UPROPERTY()
	TObjectPtr<const UPCGComputeGraph> Graph = nullptr;

	// Data collection pointer + set of pins that get data from the collection (cross barrier)
	FPCGDataForGPU DataForGPU;

	UPROPERTY()
	TMap<TObjectPtr<const UPCGSettings>, FPCGSpawnerPrimitives> MeshSpawnersToPrimitives;
};
