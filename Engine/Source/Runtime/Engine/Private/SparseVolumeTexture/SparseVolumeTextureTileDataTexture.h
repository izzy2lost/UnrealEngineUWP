// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "SparseVolumeTextureUpload.h"

class FRHICommandListBase;
class FRDGBuilder;
enum EPixelFormat : uint8;

namespace UE
{
namespace SVT
{

class FTileUploader;

// Represents the physical tile data texture that serves as backing memory for the streamed in tiles. While this is treated as a single logical texture,
// it currently supports up to two actual RHI textures.
class FTileDataTexture : public FRenderResource
{
public:
	enum class EUploaderState
	{
		Ready, Reserving, Reserved, Uploading
	};

	static constexpr uint32 PhysicalCoordMask = (1u << 24u) - 1u; // Lower 24 bits are used for storing XYZ in 8 bit each. Upper 8 bit can be used by the caller. 

	static FIntVector3 GetVolumeResolutionInTiles(int32 InNumRequiredTiles);
	static FIntVector3 GetLargestPossibleVolumeResolutionInTiles(int32 InVoxelMemSize);

	// Constructor. May change the requested ResolutionInTiles (and resulting PhysicalTilesCapacity) if it exceeds hardware limits.
	explicit FTileDataTexture(const FIntVector3& ResolutionInTiles, EPixelFormat FormatA, EPixelFormat FormatB, const FVector4f& FallbackValueA, const FVector4f& FallbackValueB);

	// Allocate a tile slot in the texture. The resulting value is a packed coordinate (8 bit per component) of the allocated slot or INDEX_NONE if the allocation failed.
	// The upper 8 bit are free to be used by the caller.
	uint32 Allocate()
	{
		check(PhysicalTilesCapacity == TileCoords.Num());
		return TileCoords.IsValidIndex(NextFreeTileCoordIndex) ? TileCoords[NextFreeTileCoordIndex++] : INDEX_NONE;
	}

	// Frees a previously allocated tile slot. The upper 8 bit (user data) are automatically cleared by this function.
	void Free(uint32 PackedPhysicalCoord)
	{
		check(PackedPhysicalCoord != INDEX_NONE)
			PackedPhysicalCoord &= PhysicalCoordMask;
		check(PhysicalTilesCapacity == TileCoords.Num());
		check(NextFreeTileCoordIndex > 0);
#if DO_GUARD_SLOW
		for (int32 i = NextFreeTileCoordIndex; i < PhysicalTilesCapacity; ++i)
		{
			check(TileCoords[i] != PackedPhysicalCoord);
		}
#endif
		TileCoords[--NextFreeTileCoordIndex] = PackedPhysicalCoord;
	}

	EUploaderState GetUploaderState() const { return UploaderState; }
	int32 GetTileCapacity() const { return PhysicalTilesCapacity; }
	int32 GetNumAvailableTiles() const { return PhysicalTilesCapacity - NextFreeTileCoordIndex; } // Number of tiles available for allocation.
	FIntVector3 GetResolutionInTiles() const { return ResolutionInTiles; }
	FTextureRHIRef GetTileDataTextureA() { return TileDataTextureARHIRef; }
	FTextureRHIRef GetTileDataTextureB() { return TileDataTextureBRHIRef; }

	// Transitions from EUploaderState::Ready (or EUploaderState::Reserved) to EUploaderState::Reserving and allows callers to call ReserveUpload() afterwards. Resets number of reserved tiles/voxels.
	void BeginReserveUpload();
	// Reserves space in the upload buffer to hold the given number of tiles and voxels in addition to all tiles and voxels reserved with prior calls to this function. Must be in EUploaderState::Reserving.
	void ReserveUpload(int32 NumTiles, int32 NumVoxelsA, int32 NumVoxelsB);
	// Transitions from EUploaderState::Reserving to EUploaderState::Reserved.
	void EndReserveUpload();
	// Transitions from EUploaderState::Reserved to EUploaderState::Uploading and allows callers to call AddUpload() afterwards.
	void BeginUpload(FRDGBuilder& GraphBuilder);
	// Returns pointers/offsets into upload buffer memory for the caller to write data into. Must be in EUploaderState::Uploading.
	FTileUploader::FAddResult AddUpload(int32 NumTiles, int32 NumVoxelsA, int32 NumVoxelsB);
	// Actually uploads the data written to the upload buffer and transitions from EUploaderState::Uploading to EUploaderState::Ready.
	void EndUpload(FRDGBuilder& GraphBuilder);

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

private:
	TUniquePtr<FTileUploader> TileUploader;
	FIntVector3 ResolutionInTiles;
	int32 PhysicalTilesCapacity;
	EPixelFormat FormatA;
	EPixelFormat FormatB;
	FVector4f FallbackValueA;
	FVector4f FallbackValueB;
	FTextureRHIRef TileDataTextureARHIRef;
	FTextureRHIRef TileDataTextureBRHIRef;
	TArray<uint32> TileCoords;
	int32 NextFreeTileCoordIndex = 0;
	EUploaderState UploaderState = EUploaderState::Ready;
	int32 NumReservedUploadTiles = 0;
	int32 NumReservedUploadVoxelsA = 0;
	int32 NumReservedUploadVoxelsB = 0;
};

}
}
