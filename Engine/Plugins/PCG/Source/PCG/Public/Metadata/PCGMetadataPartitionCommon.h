// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"

struct FPCGAttributePropertySelector;
struct FPCGContext;
class UPCGData;

namespace PCGMetadataPartitionCommon
{
	/**
	* Partition the incoming data on the given attribute.
	* Will return an array of data. Each data will contain the entries (in stable order) for a given partition.
	* @param InData - Data to partition.
	* @param InSelector - Selector on the attribute to partition.
	* @param InOptionalContext - Optional context for logging.
	* @returns Array of data.
	*/
	PCG_API TArray<UPCGData*> AttributePartition(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, FPCGContext* InOptionalContext = nullptr);

	/**
	* Generic partition for the incoming data on the given attribute.
	* Will return an array of bucket indices. Each bucket will contain the indices (in stable order) for a given partition.
	* @param InData - Data to partition, need to support attributes (spatial data or attribute set).
	* @param InSelector - Selector on the attribute to partition.
	* @param InOptionalContext - Optional context for logging.
	* @returns Array of bucket indices.
	*/
	PCG_API TArray<TArray<int32>> AttributeGenericPartition(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, FPCGContext* InOptionalContext = nullptr);
}