// Copyright Epic Games, Inc. All Rights Reserved.

#include "FX/SlatePostBufferBlur.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "RHIResources.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SlatePostBufferBlur)

void USlatePostBufferBlur::PostProcess(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FSlateRHIRenderingPolicyInterface InRenderingPolicy, UTextureRenderTarget2D* InSlatePostBuffer)
{
	if (InRenderingPolicy.IsValid())
	{
		// Explicit param copy to avoid renderthread from reading value during gamethread write
		float GaussianBlurStrengthCopy = GaussianBlurStrength;

		ENQUEUE_RENDER_COMMAND(FUpdateSlatePostBuffersWithFX_Blur)([InViewInfo, InViewportTexture, InElementWindowSize, InRenderingPolicy, InSlatePostBuffer, GaussianBlurStrengthCopy](FRHICommandListImmediate& RHICmdList)
		{
			FTexture2DRHIRef BackBuffer = USlateRHIPostBufferProcessor::GetBackbuffer_RenderThread(InViewInfo, InViewportTexture, InElementWindowSize, RHICmdList);

			if (BackBuffer)
			{
				FTexture2DRHIRef Src = USlateRHIPostBufferProcessor::GetSrcTexture_RenderThread(BackBuffer, InViewportTexture);
				FTextureReferenceRHIRef& Dst = USlateRHIPostBufferProcessor::GetDstTexture_RenderThread(InSlatePostBuffer);
				FIntPoint DstExtent = USlateRHIPostBufferProcessor::GetDstExtent_RenderThread(BackBuffer, InViewportTexture);

				InRenderingPolicy.BlurRectExternal(RHICmdList, Src, Dst, DstExtent, GaussianBlurStrengthCopy);
			}
		});
	}
}