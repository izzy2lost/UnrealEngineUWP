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

		ENQUEUE_RENDER_COMMAND(FUpdateSlatePostBuffersWithFX_Blur)([Self = this, InViewInfo, InViewportTexture, InElementWindowSize, InRenderingPolicy, InSlatePostBuffer, GaussianBlurStrengthCopy](FRHICommandListImmediate& RHICmdList)
		{
			if (Self)
			{
				FTexture2DRHIRef BackBuffer = Self->GetBackbuffer_RenderThread(InViewInfo, InViewportTexture, InElementWindowSize, RHICmdList);

				if (BackBuffer)
				{
					FTexture2DRHIRef Src = Self->GetSrcTexture_RenderThread(BackBuffer, InViewportTexture);
					FTextureReferenceRHIRef& Dst = Self->GetDstTexture_RenderThread(InSlatePostBuffer);
					FIntPoint DstExtent = Self->GetDstExtent_RenderThread(BackBuffer, InViewportTexture);

					InRenderingPolicy.BlurRectExternal(RHICmdList, Src, Dst, DstExtent, GaussianBlurStrengthCopy);
				}
			}
		});
	}
}