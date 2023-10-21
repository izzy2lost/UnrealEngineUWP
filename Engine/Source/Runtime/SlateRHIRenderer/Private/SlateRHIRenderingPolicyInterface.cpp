// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/SlateRHIRenderingPolicyInterface.h"
#include "SlateRHIRenderingPolicy.h"

FSlateRHIRenderingPolicyInterface::FSlateRHIRenderingPolicyInterface(FSlateRHIRenderingPolicy* InRenderingPolicy)
	: RenderingPolicy(InRenderingPolicy)
{
}

bool FSlateRHIRenderingPolicyInterface::IsValid() const
{
	return RenderingPolicy != nullptr;
}

bool FSlateRHIRenderingPolicyInterface::IsVertexColorInLinearSpace() const
{
	if (RenderingPolicy)
	{
		return RenderingPolicy->IsVertexColorInLinearSpace();
	}

	return false;
}

bool FSlateRHIRenderingPolicyInterface::GetApplyColorDeficiencyCorrection() const
{
	if (RenderingPolicy)
	{
		return RenderingPolicy->GetApplyColorDeficiencyCorrection();
	}

	return false;
}

void FSlateRHIRenderingPolicyInterface::BlurRectExternal(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef BlurSrc, FTextureReferenceRHIRef& BlurDst, FIntPoint DstExtent, float BlurStrength) const
{
	if (RenderingPolicy)
	{
		RenderingPolicy->BlurRectExternal(RHICmdList, BlurSrc, BlurDst, DstExtent, BlurStrength);
	}
}