// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeAsyncTextureReadback.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

void FLandscapeAsyncTextureReadback::StartReadback_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef RDGTexture)
{
	check(!bAsyncReadbackSubmitOnRenderThread && !AsyncReadback);
	check(RDGTexture->Desc.Format == PF_B8G8R8A8);
	AsyncReadback = MakeUnique<FRHIGPUTextureReadback>(TEXT("LandscapeGrassReadback"));
	AddEnqueueCopyPass(GraphBuilder, AsyncReadback.Get(), RDGTexture);
	FIntVector Size = RDGTexture->Desc.GetSize();
	TextureWidth = Size.X;
	TextureHeight = Size.Y;
	check(Size.Z == 1);

	bAsyncReadbackSubmitOnRenderThread = true;
}

void FLandscapeAsyncTextureReadback::FinishReadback_RenderThread()
{
	check(bAsyncReadbackSubmitOnRenderThread && AsyncReadback.IsValid());
	int32 RowPitchInPixels = 0;
	int32 BufferHeight = 0;
	void* SrcData = AsyncReadback->Lock(RowPitchInPixels, &BufferHeight);
	check(SrcData);
	check(RowPitchInPixels >= TextureWidth);
	check(BufferHeight >= TextureHeight);

	// copy into ReadbackResults
	ReadbackResults.SetNumUninitialized(TextureWidth * TextureHeight);

	// OpenGL does not really support BGRA images and uses channnel swizzling to emulate them
	// so when we read them back we get internal RGBA representation
	const bool bSwapRBChannels = IsOpenGLPlatform(GMaxRHIShaderPlatform);

	if (!bSwapRBChannels && TextureWidth == RowPitchInPixels)
	{
		memcpy(ReadbackResults.GetData(), SrcData, TextureWidth * TextureHeight * sizeof(FColor));
	}
	else
	{
		// copy row by row
		FColor* Dst = ReadbackResults.GetData();
		FColor* Src = (FColor*)SrcData;
		if (bSwapRBChannels)
		{
			for (int y = 0; y < TextureHeight; y++)
			{
				for (int x = 0; x < TextureWidth; x++)
				{
					// swap B and R channels when copying
					Dst->B = Src->R;
					Dst->G = Src->G;
					Dst->R = Src->B;
					Dst->A = Src->A;
					Dst++;
					Src++;
				}
				Src += RowPitchInPixels - TextureWidth;
			}
		}
		else
		{
			for (int y = 0; y < TextureHeight; y++)
			{
				memcpy(Dst, Src, TextureWidth * sizeof(FColor));
				Dst += TextureWidth;
				Src += RowPitchInPixels;
			}
		}
	}

	AsyncReadback->Unlock();
	AsyncReadback.Reset();

	FPlatformMisc::MemoryBarrier();
	bAsyncReadbackCompleteOnRenderThread = true;
}

bool FLandscapeAsyncTextureReadback::CheckAndUpdate()
{
	if (bAsyncReadbackCompleteOnRenderThread)
	{
		// already done
		return true;
	}

	// not done yet -- queue update check on render thread
	FLandscapeAsyncTextureReadback* Readback = this;
	ENQUEUE_RENDER_COMMAND(FLandscapeAsyncTextureReadback_CheckAndUpdate)(
		[Readback](FRHICommandListImmediate& RHICmdList)
		{
			if (Readback->bAsyncReadbackCompleteOnRenderThread)
			{
				// Actually finished already.. game thread was just asking too early.
				// No need to do anything, game thread will advance next time it checks.
			}
			else if (Readback->bAsyncReadbackSubmitOnRenderThread)
			{
				// Readback was submit but not found to be complete yet -- let's check if it completed.
				check(Readback->AsyncReadback.IsValid());
				if (Readback->AsyncReadback->IsReady())
				{
					Readback->FinishReadback_RenderThread();
				}
			}
			else
			{
				// not submit yet.. nothing to do
			}
		});

	return false;
}

void FLandscapeAsyncTextureReadback::QueueDeletionFromGameThread()
{
	check(IsInGameThread());
	check(bAsyncReadbackCompleteOnRenderThread);

	FLandscapeAsyncTextureReadback* Readback = this;
	ENQUEUE_RENDER_COMMAND(FLandscapeAsyncTextureReadback_CheckAndUpdate)(
		[Readback](FRHICommandListImmediate& RHICmdList)
		{
			delete Readback;
		});
}
