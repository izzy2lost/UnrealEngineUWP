// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"

#include "CoreMinimal.h"
#include "Misc/Optional.h"

#include "PCGGridDescriptor.generated.h"

/**
* Descriptor struct used to determine where to output generated resources
*/
USTRUCT()
struct FPCGGridDescriptor
{
	GENERATED_BODY()

private:
	UPROPERTY()
	uint32 GridSize = 0;
		
	UPROPERTY()
	bool bIs2DGrid = true;

	UPROPERTY()
	bool bIsRuntime = false;
		
public:
	FPCGGridDescriptor& SetGridSize(uint32 InGridSize) { check(!Hash.IsSet()); GridSize = InGridSize; return *this; }
	uint32 GetGridSize() const { return GridSize; }
		
	EPCGHiGenGrid GetHiGenGrid() const { return PCGHiGenGrid::GridSizeToGrid(GetGridSize()); }

	FPCGGridDescriptor& SetIs2DGrid(bool bInIs2DGrid) { check(!Hash.IsSet()); bIs2DGrid = bInIs2DGrid; return *this; }
	bool Is2DGrid() const { return bIs2DGrid; }

	FPCGGridDescriptor& SetIsRuntime(bool bInIsRuntime) { check(!Hash.IsSet()); bIsRuntime = bInIsRuntime; return *this; }
	bool IsRuntime() const { return bIsRuntime; }

	bool operator==(const FPCGGridDescriptor& Other) const;
	bool operator!=(const FPCGGridDescriptor& Other) const { return !(*this == Other); }
		
	friend uint32 GetTypeHash(const FPCGGridDescriptor& Descriptor)
	{
		if (!Descriptor.Hash.IsSet())
		{
			Descriptor.Hash = Descriptor.ComputeHash();
		}

		return Descriptor.Hash.GetValue();
	}

private:
	uint32 ComputeHash() const;

	mutable TOptional<uint32> Hash;
};

/**
 * Describes one entry in a Grid
 */
USTRUCT()
struct FPCGGridCellDescriptor
{
	GENERATED_BODY()

	FPCGGridCellDescriptor() = default;
	FPCGGridCellDescriptor(const FPCGGridDescriptor& InDescriptor, const FIntVector& InGridCoords)
		: Descriptor(InDescriptor), GridCoords(InGridCoords)
	{}

	UPROPERTY()
	FPCGGridDescriptor Descriptor;

	/** The specific grid cell this actor lives in. */
	UPROPERTY(VisibleAnywhere, Category = Debug)
	FIntVector GridCoords = FIntVector::ZeroValue;

	bool operator==(const FPCGGridCellDescriptor& InOther) const;
	friend uint32 GetTypeHash(const FPCGGridCellDescriptor& In);
};

