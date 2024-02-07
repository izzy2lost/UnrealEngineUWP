// Copyright Epic Games, Inc. All Rights Reserved.

#include "SparseVolumeTextureTileDataTexture.h"
#include "SparseVolumeTextureUtility.h"
#include "SparseVolumeTextureStreamingManager.h" // LogSparseVolumeTextureStreamingManager

namespace UE
{
namespace SVT
{

FIntVector3 FTileDataTexture::GetVolumeResolutionInTiles(int32 InNumRequiredTiles)
{
	int32 TileVolumeResolutionCube = 1;
	while (TileVolumeResolutionCube * TileVolumeResolutionCube * TileVolumeResolutionCube < InNumRequiredTiles)
	{
		TileVolumeResolutionCube++;				// We use a simple loop to compute the minimum resolution of a cube to store all the tile data
	}
	FIntVector3 TileDataVolumeResolution = FIntVector3(TileVolumeResolutionCube, TileVolumeResolutionCube, TileVolumeResolutionCube);

	// Trim volume to reclaim some space
	while ((TileDataVolumeResolution.X * TileDataVolumeResolution.Y * (TileDataVolumeResolution.Z - 1)) > InNumRequiredTiles)
	{
		TileDataVolumeResolution.Z--;
	}
	while ((TileDataVolumeResolution.X * (TileDataVolumeResolution.Y - 1) * TileDataVolumeResolution.Z) > InNumRequiredTiles)
	{
		TileDataVolumeResolution.Y--;
	}
	while (((TileDataVolumeResolution.X - 1) * TileDataVolumeResolution.Y * TileDataVolumeResolution.Z) > InNumRequiredTiles)
	{
		TileDataVolumeResolution.X--;
	}

	return TileDataVolumeResolution;
}

FIntVector3 FTileDataTexture::GetLargestPossibleVolumeResolutionInTiles(int32 InVoxelMemSize)
{
	const int64 TileMemSize = SVT::NumVoxelsPerPaddedTile * InVoxelMemSize;
	const int64 NumMaxTiles = SVT::MaxResourceSize / TileMemSize;

	// Find a cube with a volume as close to NumMaxTiles as possible
	int32 TileVolumeResolutionCube = 1;
	while (((TileVolumeResolutionCube + 1) * (TileVolumeResolutionCube + 1) * (TileVolumeResolutionCube + 1)) <= NumMaxTiles)
	{
		++TileVolumeResolutionCube;
	}

	// Try to add to the sides to get closer to NumMaxTiles
	FIntVector3 ResolutionInTiles = FIntVector3(TileVolumeResolutionCube, TileVolumeResolutionCube, TileVolumeResolutionCube);
	if (((ResolutionInTiles.X + 1) * ResolutionInTiles.Y * ResolutionInTiles.Z) <= NumMaxTiles)
	{
		++ResolutionInTiles.X;
	}
	if ((ResolutionInTiles.X * (ResolutionInTiles.Y + 1) * ResolutionInTiles.Z) <= NumMaxTiles)
	{
		++ResolutionInTiles.Y;
	}
	if ((ResolutionInTiles.X * ResolutionInTiles.Y * (ResolutionInTiles.Z + 1)) <= NumMaxTiles)
	{
		++ResolutionInTiles.Z;
	}

	const FIntVector3 ResolutionInVoxels = ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
	check(IsInBounds(ResolutionInVoxels, FIntVector3::ZeroValue, FIntVector3(SVT::MaxVolumeTextureDim + 1)));
	check(ResolutionInVoxels.X <= SVT::MaxVolumeTextureDim && ResolutionInVoxels.Y <= SVT::MaxVolumeTextureDim && ResolutionInVoxels.Z <= SVT::MaxVolumeTextureDim);
	check(((int64)ResolutionInVoxels.X * (int64)ResolutionInVoxels.Y * (int64)ResolutionInVoxels.Z * (int64)InVoxelMemSize) <= SVT::MaxResourceSize);

	return ResolutionInTiles;
}

FTileDataTexture::FTileDataTexture(const FIntVector3& InResolutionInTiles, EPixelFormat InFormatA, EPixelFormat InFormatB, const FVector4f& InFallbackValueA, const FVector4f& InFallbackValueB)
	: TileUploader(MakeUnique<FTileUploader>()),
	ResolutionInTiles(InResolutionInTiles),
	PhysicalTilesCapacity(InResolutionInTiles.X* InResolutionInTiles.Y* InResolutionInTiles.Z),
	FormatA(InFormatA),
	FormatB(InFormatB),
	FallbackValueA(InFallbackValueA),
	FallbackValueB(InFallbackValueB)
{
	const int64 MaxFormatSize = FMath::Max(GPixelFormats[FormatA].BlockBytes, GPixelFormats[FormatB].BlockBytes);
	const FIntVector3 LargestPossibleResolutionInTiles = GetLargestPossibleVolumeResolutionInTiles(MaxFormatSize);
	const int32 LargestPossiblePhysicalTilesCapacity = LargestPossibleResolutionInTiles.X * LargestPossibleResolutionInTiles.Y * LargestPossibleResolutionInTiles.Z;

	// Ensure that the tile data texture(s) do not exceed the memory size and resolution limits.
	if (PhysicalTilesCapacity > LargestPossiblePhysicalTilesCapacity
		|| (ResolutionInTiles.X * SPARSE_VOLUME_TILE_RES_PADDED) > SVT::MaxVolumeTextureDim
		|| (ResolutionInTiles.Y * SPARSE_VOLUME_TILE_RES_PADDED) > SVT::MaxVolumeTextureDim
		|| (ResolutionInTiles.Z * SPARSE_VOLUME_TILE_RES_PADDED) > SVT::MaxVolumeTextureDim)
	{
		ResolutionInTiles = LargestPossibleResolutionInTiles;
		PhysicalTilesCapacity = LargestPossiblePhysicalTilesCapacity;

		UE_LOG(LogSparseVolumeTextureStreamingManager, Warning, TEXT("Requested SparseVolumeTexture tile data texture resolution (in tiles) (%i, %i, %i) exceeds the resource size limit. Using the maximum value of (%i, %i. %i) instead."),
			InResolutionInTiles.X, InResolutionInTiles.Y, InResolutionInTiles.Z,
			LargestPossibleResolutionInTiles.X, LargestPossibleResolutionInTiles.Y, LargestPossibleResolutionInTiles.Z);
	}

	const FIntVector3 Resolution = ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
	check(Resolution.X <= SVT::MaxVolumeTextureDim && Resolution.Y <= SVT::MaxVolumeTextureDim && Resolution.Z <= SVT::MaxVolumeTextureDim);
	check(((int64)Resolution.X * (int64)Resolution.Y * (int64)Resolution.Z * (int64)GPixelFormats[FormatA].BlockBytes) <= SVT::MaxResourceSize);
	check(((int64)Resolution.X * (int64)Resolution.Y * (int64)Resolution.Z * (int64)GPixelFormats[FormatB].BlockBytes) <= SVT::MaxResourceSize);

	TileCoords.SetNum(PhysicalTilesCapacity);

	int32 TileCoordsIndex = 0;
	for (int32 Z = 0; Z < ResolutionInTiles.Z; ++Z)
	{
		for (int32 Y = 0; Y < ResolutionInTiles.Y; ++Y)
		{
			for (int32 X = 0; X < ResolutionInTiles.X; ++X)
			{
				uint32 PackedCoord = 0;
				PackedCoord |= (X & 0xFFu);
				PackedCoord |= (Y & 0xFFu) << 8u;
				PackedCoord |= (Z & 0xFFu) << 16u;
				TileCoords[TileCoordsIndex++] = PackedCoord;
			}
		}
	}
	check(TileCoordsIndex == PhysicalTilesCapacity);
}

void FTileDataTexture::BeginReserveUpload()
{
	check(UploaderState == EUploaderState::Ready || UploaderState == EUploaderState::Reserved);
	UploaderState = EUploaderState::Reserving;
	NumReservedUploadTiles = 0;
	NumReservedUploadVoxelsA = 0;
	NumReservedUploadVoxelsB = 0;
}

void FTileDataTexture::ReserveUpload(int32 NumTiles, int32 NumVoxelsA, int32 NumVoxelsB)
{
	check(UploaderState == EUploaderState::Reserving);
	NumReservedUploadTiles += NumTiles;
	NumReservedUploadVoxelsA += NumVoxelsA;
	NumReservedUploadVoxelsB += NumVoxelsB;
}

void FTileDataTexture::EndReserveUpload()
{
	check(UploaderState == EUploaderState::Reserving);
	UploaderState = EUploaderState::Reserved;
}

void FTileDataTexture::BeginUpload(FRDGBuilder& GraphBuilder)
{
	check(UploaderState == EUploaderState::Reserved);
	TileUploader->Init(GraphBuilder, NumReservedUploadTiles, NumReservedUploadVoxelsA, NumReservedUploadVoxelsB, FormatA, FormatB);
	UploaderState = EUploaderState::Uploading;
}

FTileUploader::FAddResult FTileDataTexture::AddUpload(int32 NumTiles, int32 NumVoxelsA, int32 NumVoxelsB)
{
	check(UploaderState == EUploaderState::Uploading);
	return TileUploader->Add_GetRef(NumTiles, NumVoxelsA, NumVoxelsB);
}

void FTileDataTexture::EndUpload(FRDGBuilder& GraphBuilder)
{
	check(UploaderState == EUploaderState::Uploading);
	TileUploader->ResourceUploadTo(GraphBuilder, TileDataTextureARHIRef, TileDataTextureBRHIRef, FallbackValueA, FallbackValueB);
	UploaderState = EUploaderState::Ready;
}

void FTileDataTexture::InitRHI(FRHICommandListBase&)
{
	const FIntVector3 Resolution = ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
	if (FormatA != PF_Unknown)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PhysicalTileDataA.RHITexture"), Resolution.X, Resolution.Y, Resolution.Z, FormatA)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
		TileDataTextureARHIRef = RHICreateTexture(Desc);
	}
	if (FormatB != PF_Unknown)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PhysicalTileDataB.RHITexture"), Resolution.X, Resolution.Y, Resolution.Z, FormatB)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
		TileDataTextureBRHIRef = RHICreateTexture(Desc);
	}
}

void FTileDataTexture::ReleaseRHI()
{
}

}
}