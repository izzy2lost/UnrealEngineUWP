// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/ShaderResourceManager.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingPolicy.h"

class FSlateRHIRenderingPolicy;

/**
 * Class used to expose only limited parts of the FSlateRenderingPolicy. 
 * Not an interface to be implemented.
 */
class SLATERHIRENDERER_API FSlateRHIRenderingPolicyInterface
{
public:
	FSlateRHIRenderingPolicyInterface(FSlateRHIRenderingPolicy* InRenderingPolicy);

	bool IsValid() const;
	bool IsVertexColorInLinearSpace() const;
	bool GetApplyColorDeficiencyCorrection() const;

	void BlurRectExternal(FRHICommandListImmediate& RHICmdList, FTexture2DRHIRef BlurSrc, FTextureReferenceRHIRef& BlurDst, FIntPoint DstExtent, float BlurStrength) const;

private:

	/** Rendering policy we are an interface to */
	FSlateRHIRenderingPolicy* RenderingPolicy;
};
