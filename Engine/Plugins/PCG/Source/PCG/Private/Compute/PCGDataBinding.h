// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "Compute/PCGDataForGPU.h"

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

	UPROPERTY()
	int32 SelectorAttributeId = -1;

	UPROPERTY()
	TArray<uint32> PrimitiveStringKeys;
};

UCLASS(Transient, Category = PCG)
class UPCGDataBinding : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(const UPCGComputeGraph* InComputeGraph, TWeakObjectPtr<UPCGComponent> InSourceComponent, const FPCGDataCollection& InComputeGraphElementInputData, const TMap<FName, FPCGKernelAttributeIDAndType>& InStaticAttributeTable);

	/** Get the global attribute information for this graph instance. */
	const TMap<FName, FPCGKernelAttributeIDAndType>& GetAttributeLookupTable() const { return GlobalAttributeLookupTable; }

	/** Get the set of all unique strings entering the compute graph. */
	const TArray<FString>& GetStringTable() const { return StringTable; }

private:
	void InitializeInputData(const FPCGDataCollection& InComputeGraphElementInputData);

	void BuildStringTable();

	/** Some attributes in table may be missing type information that we could not infer statically. Fill in missing types from the execution-time data. */
	void FillInMissingAttributeTableTypes(const FPCGDataCollection& InComputeGraphElementInputData);

	void DebugLogDataDescriptions();

public:
	UPROPERTY()
	TWeakObjectPtr<UPCGComponent> SourceComponent;

	UPROPERTY()
	FPCGDataCollection OutputDataCollection;

	UPROPERTY()
	TObjectPtr<const UPCGComputeGraph> Graph = nullptr;

	// Data collection pointer + set of pins that get data from the collection (cross barrier)
	UPROPERTY()
	FPCGDataForGPU DataForGPU;

	UPROPERTY()
	TMap<TObjectPtr<const UPCGSettings>, FPCGSpawnerPrimitives> MeshSpawnersToPrimitives;

protected:
	UPROPERTY()
	TMap<FName /* Attribute name */, FPCGKernelAttributeIDAndType> GlobalAttributeLookupTable;

	UPROPERTY()
	TArray<FString> StringTable;
};
