// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIReservedResourceTests.h"
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
