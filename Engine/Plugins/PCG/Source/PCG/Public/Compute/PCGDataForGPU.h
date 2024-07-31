// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGData.h"

#include "PCGDataForGPU.generated.h"

class UPCGMetadata;
class UPCGPin;
class UPCGSettings;

enum class EPCGUnpackDataCollectionResult
{
	Success,
	DataMismatch
};

UENUM()
enum class EPCGKernelAttributeType : uint8
{
	Bool,
	Int,
	Float,
	Float2,
	Float3,
	Float4,
	Rotator,
	Quat,
	Transform
};

USTRUCT()
struct FPCGKernelAttributeKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	EPCGKernelAttributeType Type = EPCGKernelAttributeType::Float;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName Name = NAME_None;

	bool operator==(const FPCGKernelAttributeKey& Other) const;
	friend uint32 GetTypeHash(const FPCGKernelAttributeKey& In);
};

struct FPCGKernelAttributeDesc
{
	explicit FPCGKernelAttributeDesc(int32 InIndex, EPCGKernelAttributeType InType, FName InName)
		: Index(InIndex)
		, Type(InType)
		, Name(InName)
	{
	}

	int32 Index = INDEX_NONE;
	EPCGKernelAttributeType Type = EPCGKernelAttributeType::Float;
	FName Name = NAME_None;

	bool operator==(const FPCGKernelAttributeDesc& Other) const;
};

struct FPCGDataDesc
{
	FPCGDataDesc(EPCGDataType InType, int32 InElementCount);
	FPCGDataDesc(const UPCGData* Data, const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable);

	uint32 ComputePackedSize() const;

	EPCGDataType Type = EPCGDataType::Point;
	TArray<FPCGKernelAttributeDesc> AttributeDescs;
	int32 ElementCount = 0;

private:
	void InitializeAttributeDescs(const UPCGMetadata* Metadata, const TMap<FPCGKernelAttributeKey, int32>& GlobalAttributeLookupTable = {});
};

struct FPCGDataCollectionDesc
{
	static FPCGDataCollectionDesc BuildFromInputDataCollectionAndInputPinLabel(
		const FPCGDataCollection& InDataCollection,
		FName InputPinLabel,
		const TMap<FPCGKernelAttributeKey, int32>& InAttributeLookupTable);

	/** Computes the size (in bytes) of the data collection after packing. Also produces the offset (in bytes) for each data in the packed collection. */
	uint32 ComputePackedSize(TArray<uint32>* OutDataAddresses = nullptr) const;

	/** Pack a data collection into the GPU data format. DataDescs defines which attributes are packed. */
	void PackDataCollection(const FPCGDataCollection& InDataCollection, FName InPin, TArray<uint32>& OutPackedDataCollection) const;

	/** Allocates the correct size and sets up header. Initializes data count to 0, which kernel will then overwrite if it executes at least one thread. */
	void PrepareBufferForKernelOutput(TArray<uint32>& OutPackedDataCollection);

	/** Unpack a buffer of 8-bit uints to a data collection. */
	EPCGUnpackDataCollectionResult UnpackDataCollection(const TArray<uint8>& InPackedData, FName InPin, FPCGDataCollection& OutDataCollection) const;

	/** Compute total number of processing elements of the given type. */
	uint32 ComputeDataElementCount(EPCGDataType InDataType) const;

	/** Aggregate another data description. */
	void Combine(const FPCGDataCollectionDesc& Other);

	TArray<FPCGDataDesc> DataDescs;
};

struct FPCGDataForGPU
{
	// All the node input pins that have edges that cross from CPU to GPU.
	TSet<const UPCGPin*> InputPins;
	TMap<TObjectPtr<const UPCGPin>, FName> InputPinLabelAliases;

	// Since the compute graph is collapsed to a single element, all data crossing from CPU to GPU is in a single collection.
	FPCGDataCollection InputDataCollection;
};
