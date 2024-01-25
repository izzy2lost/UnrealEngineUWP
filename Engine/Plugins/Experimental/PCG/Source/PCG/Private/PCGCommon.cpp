// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCommon.h"

namespace PCGFeatureSwitches
{

	TAutoConsoleVariable<bool> CVarCheckSamplerMemory{
		TEXT("pcg.CheckSamplerMemory"),
		true,
		TEXT("Checks expected memory size consumption prior to performing sampling operations")
	};

}

namespace PCGHiGenGrid
{
	bool IsValidGridSize(uint32 InGridSize)
	{
		// Must be a power of 2 (in m) and within the valid range
		// TODO: support other units
		return FMath::IsPowerOfTwo(InGridSize / 100)
			&& InGridSize >= GridToGridSize(EPCGHiGenGrid::GridMin)
			&& InGridSize <= GridToGridSize(EPCGHiGenGrid::GridMax);
	}

	bool IsValidGrid(EPCGHiGenGrid InGrid)
	{
		// Check the bitmask value is within range
		return InGrid >= EPCGHiGenGrid::GridMin && static_cast<uint32>(InGrid) < 2 * static_cast<uint32>(EPCGHiGenGrid::GridMax);
	}

	bool IsValidGridOrUninitialized(EPCGHiGenGrid InGrid)
	{
		return IsValidGrid(InGrid) || InGrid == EPCGHiGenGrid::Uninitialized;
	}

	uint32 GridToGridSize(EPCGHiGenGrid InGrid)
	{
		const uint32 GridAsUint = static_cast<uint32>(InGrid);
		ensure(FMath::IsPowerOfTwo(GridAsUint));
		// TODO: support other units
		return IsValidGrid(InGrid) ? (GridAsUint * 100) : UninitializedGridSize();
	}

	EPCGHiGenGrid GridSizeToGrid(uint32 InGridSize)
	{
		if (InGridSize == UnboundedGridSize())
		{
			return EPCGHiGenGrid::Unbounded;
		}
		// TODO: support other units
		return ensure(IsValidGridSize(InGridSize)) ? static_cast<EPCGHiGenGrid>(InGridSize / 100) : EPCGHiGenGrid::Uninitialized;
	}

	uint32 UninitializedGridSize()
	{
		return static_cast<uint32>(EPCGHiGenGrid::Uninitialized);
	}

	uint32 UnboundedGridSize()
	{
		return static_cast<uint32>(EPCGHiGenGrid::Unbounded);
	}
}

double FPCGRuntimeGenerationRadii::GetGenerationRadiusFromGrid(EPCGHiGenGrid Grid) const
{
	switch (Grid)
	{
		case EPCGHiGenGrid::Grid4: return GenerationRadius400;
		case EPCGHiGenGrid::Grid8: return GenerationRadius800;
		case EPCGHiGenGrid::Grid16: return GenerationRadius1600;
		case EPCGHiGenGrid::Grid32: return GenerationRadius3200;
		case EPCGHiGenGrid::Grid64: return GenerationRadius6400;
		case EPCGHiGenGrid::Grid128: return GenerationRadius12800;
		case EPCGHiGenGrid::Grid256: return GenerationRadius25600;
		case EPCGHiGenGrid::Grid512: return GenerationRadius51200;
		case EPCGHiGenGrid::Grid1024: return GenerationRadius102400;
		case EPCGHiGenGrid::Grid2048: return GenerationRadius204800;
		case EPCGHiGenGrid::Unbounded: return GenerationRadius;
	}

	ensure(false);
	return 0;
}

double FPCGRuntimeGenerationRadii::GetCleanupRadiusFromGrid(EPCGHiGenGrid Grid) const
{
	return GetGenerationRadiusFromGrid(Grid) * CleanupRadiusMultiplier;
}

namespace PCGDelegates
{
#if WITH_EDITOR
	FOnInstanceLayoutChanged OnInstancedPropertyBagLayoutChanged;
#endif
} // PCGDelegates

namespace PCGPinIdHelpers
{
	FPCGPinId NodeIdAndPinIndexToPinId(FPCGTaskId NodeId, uint64 PinIndex)
	{
		// Construct a unique ID from node index and pin index.
		ensure(PinIndex < PCGPinIdHelpers::MaxOutputPins);
		return (NodeId * PCGPinIdHelpers::PinActiveBitmaskSize) + (PinIndex % PCGPinIdHelpers::PinActiveBitmaskSize);
	}

	FPCGPinId NodeIdToPinId(FPCGTaskId NodeId)
	{
		// Use (max pins - 1) to make a special pin ID that is not associated to a specific node pin.
		return (NodeId * PCGPinIdHelpers::PinActiveBitmaskSize) + PCGPinIdHelpers::MaxOutputPins;
	}

	FPCGPinId OffsetNodeIdInPinId(FPCGPinId PinId, uint64 NodeIDOffset)
	{
		return NodeIDOffset * PCGPinIdHelpers::PinActiveBitmaskSize + PinId;
	}

	FPCGTaskId GetNodeIdFromPinId(FPCGPinId PinId)
	{
		return PinId / PCGPinIdHelpers::PinActiveBitmaskSize;
	}

	uint64 GetPinIndexFromPinId(FPCGPinId PinId)
	{
		return PinId % PCGPinIdHelpers::PinActiveBitmaskSize;
	}
}
