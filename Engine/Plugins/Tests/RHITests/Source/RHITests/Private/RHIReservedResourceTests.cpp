// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIReservedResourceTests.h"
#include "RHIBufferTests.h" // for VerifyBufferContents
#include "CommonRenderResources.h"
#include "RenderCaptureInterface.h"
#include "RHIStaticStates.h"

bool FRHIReservedResourceTests::Test_ReservedResource_CreateTexture(FRHICommandListImmediate& RHICmdList)
{
	if (!GRHIGlobals.ReservedResources.Supported)
	{
		return true;
	}

	const ETextureCreateFlags CreateFlags =
		  TexCreate_ShaderResource
		| TexCreate_ReservedResource
		| TexCreate_ImmediateCommit;

	const EPixelFormat TestFormats[] =
	{
		PF_DXT1,
		PF_R8,
		PF_R32_UINT,
	};

	struct FTestTextureConfig
	{
		FIntPoint Extent = FIntPoint(1,1);
		int32 NumMips = 1;
		int32 ArraySize = 1;
	};

	const int32 TileSize = GRHIGlobals.ReservedResources.TextureArrayMinimumMipDimension;

	FTestTextureConfig TestConfigs[] =
	{
		// Texture arrays
		FTestTextureConfig{.Extent = FIntPoint(TileSize * 8, TileSize),  .NumMips = 1, .ArraySize = 2},
		FTestTextureConfig{.Extent = FIntPoint(TileSize * 2, TileSize),  .NumMips = 1, .ArraySize = 3},

		// Regular textures
		FTestTextureConfig{.Extent = FIntPoint(4096, 128), .NumMips = 1, .ArraySize = 1},
		FTestTextureConfig{.Extent = FIntPoint(128, 128),  .NumMips = 1, .ArraySize = 1},
		FTestTextureConfig{.Extent = FIntPoint(512, 128),  .NumMips = 1, .ArraySize = 1},
		FTestTextureConfig{.Extent = FIntPoint(512, 128),  .NumMips = 1, .ArraySize = 1},
		FTestTextureConfig{.Extent = FIntPoint(4, 4),      .NumMips = 1, .ArraySize = 1},

#if 0 // TODO: as of 2023-10-18, reserved textures with mips are not supported
		FTestTextureConfig{.Extent = FIntPoint(128, 128),  .NumMips = 2, .ArraySize = 2},
		FTestTextureConfig{.Extent = FIntPoint(512, 128),  .NumMips = 2, .ArraySize = 2},
		FTestTextureConfig{.Extent = FIntPoint(512, 128),  .NumMips = 5, .ArraySize = 2},
		FTestTextureConfig{.Extent = FIntPoint(4, 4),      .NumMips = 2, .ArraySize = 2},
#endif
	};

	// Try to create resources of various formats and dimensions

	for (FTestTextureConfig Config : TestConfigs)
	{
		for (EPixelFormat Format : TestFormats)
		{
			FRHITextureCreateDesc Desc;

			if (Config.ArraySize > 1)
			{
				Desc = FRHITextureCreateDesc::Create2DArray(TEXT("TestReservedTexture2DArray"));
				Desc.SetArraySize(Config.ArraySize);
			}
			else
			{
				Desc = FRHITextureCreateDesc::Create2D(TEXT("TestReservedTexture2D"));
			}

			Desc.SetFlags(CreateFlags)
				.SetExtent(Config.Extent)
				.SetNumMips(Config.NumMips)
				.SetFormat(Format)
				.SetInitialState(ERHIAccess::SRVCompute);

			FTextureRHIRef Texture = RHICreateTexture(Desc);
		}
	}

	return true;
}

bool FRHIReservedResourceTests::Test_ReservedResource_CreateBuffer(FRHICommandListImmediate& RHICmdList)
{
	if (!GRHIGlobals.ReservedResources.Supported)
	{
		return true;
	}

	// Simply try to create reserved buffers of different types and sizes to see if we hit any unexpected paths in the RHI

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TestSmallReservedVertexBuffer"));
		FBufferRHIRef Buffer = RHICmdList.CreateBuffer(32768, BUF_ReservedResource | BUF_VertexBuffer, 4, ERHIAccess::CopyDest, CreateInfo);
	}

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TestSmallReservedUAV"));
		FBufferRHIRef Buffer = RHICmdList.CreateBuffer(32768, BUF_ReservedResource | BUF_UnorderedAccess | BUF_ShaderResource, 4, ERHIAccess::UAVGraphics, CreateInfo);
	}

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TestReservedVertexBuffer"));
		FBufferRHIRef Buffer = RHICmdList.CreateBuffer(32 * 1024 * 1024, BUF_ReservedResource | BUF_VertexBuffer, 4, ERHIAccess::CopyDest, CreateInfo);
	}

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TestReservedAccelerationStructureBuffer"));
		FBufferRHIRef Buffer = RHICmdList.CreateBuffer(32 * 1024 * 1024, BUF_ReservedResource | BUF_AccelerationStructure, 4, ERHIAccess::BVHWrite, CreateInfo);
	}

	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TestReservedRayTracingScratchBuffer"));
		FBufferRHIRef Buffer = RHICmdList.CreateBuffer(32 * 1024 * 1024, BUF_ReservedResource | BUF_RayTracingScratch, 4, ERHIAccess::UAVCompute, CreateInfo);
	}

	return true;
}

static void CommitBuffer(FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint64 CommitSize, ERHIAccess StateBefore, ERHIAccess StateAfter)
{
	FRHITransitionInfo TransitionInfo(Buffer, StateBefore, StateAfter, FRHICommitResourceInfo(CommitSize));

	TArrayView<const FRHITransitionInfo> TransitionInfos = MakeArrayView(&TransitionInfo, 1);

	FRHITransitionCreateInfo CreateInfo(
		ERHIPipeline::Graphics, ERHIPipeline::Graphics,
		ERHITransitionCreateFlags::None, TransitionInfos);

	const FRHITransition* Transition = RHICreateTransition(CreateInfo);

	RHICmdList.BeginTransition(Transition);
	RHICmdList.EndTransition(Transition);
}

bool FRHIReservedResourceTests::Test_ReservedResource_CommitBuffer(FRHICommandListImmediate& RHICmdList)
{
	if (!GRHIGlobals.ReservedResources.Supported)
	{
		return true;
	}

	const int32 TileSizeInBytes = GRHIGlobals.ReservedResources.TileSizeInBytes;
	const int32 BufferSizeInBytes = TileSizeInBytes * 128;

	FRHIResourceCreateInfo CreateInfo(TEXT("TestReservedBufferExplicitCommit"));

	FBufferRHIRef Buffer = RHICmdList.CreateBuffer(BufferSizeInBytes,
		BUF_ReservedResource | BUF_UnorderedAccess | BUF_ShaderResource | BUF_SourceCopy,
		4, ERHIAccess::UAVCompute, CreateInfo);

	FUnorderedAccessViewRHIRef BufferUAV = RHICmdList.CreateUnorderedAccessView(Buffer,
		FRHIViewDesc::CreateBufferUAV()
		.SetType(FRHIViewDesc::EBufferType::Typed)
		.SetFormat(PF_R32_UINT));

	// Commit half of the resource, leaving the tail unmapped. 
	// The RHI follows D3D12 Tier 2 Reserved Resource semantics:
	// - Unmapped page writes are discarded
	// - Unmapped page reads return 0

	const int32 CommitSizeInBytes = BufferSizeInBytes / 2;
	CommitBuffer(RHICmdList, Buffer, CommitSizeInBytes, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute);

	RHICmdList.ClearUAVUint(BufferUAV, FUintVector4(~0u));

	RHICmdList.Transition(FRHITransitionInfo(Buffer, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));
	
	FRHIBuffer* Buffers[] = { Buffer.GetReference() };
	bool bSucceeded = FRHIBufferTests::VerifyBufferContents(TEXT("Test_ReservedResource_CommitBuffer"), RHICmdList, Buffers, 
		[BufferSizeInBytes, CommitSizeInBytes](int32 BufferIndex, void* Ptr, uint32 NumBytes)
		{
			uint64 ExpectedCommittedValue = ~0ull;

			uint32 CommittedSizeInElements = CommitSizeInBytes / sizeof(ExpectedCommittedValue);
			uint32 TotalSizeInElements = BufferSizeInBytes / sizeof(ExpectedCommittedValue);
			const uint64* BufferData = reinterpret_cast<const uint64*>(Ptr);

			for (uint32 i = 0; i < CommittedSizeInElements; ++i)
			{
				if (BufferData[i] != ExpectedCommittedValue)
				{
					return false;
				}
			}

			// We follow the D3D convention for unmapped page access: writes are no-op, reads return 0
			const uint64 ExpectedTailValue = 0;
			for (uint32 i = CommittedSizeInElements; i < TotalSizeInElements; ++i)
			{
				if (BufferData[i] != ExpectedTailValue)
				{
					return false;
				}
			}

			return true;
		});

	return bSucceeded;
}

