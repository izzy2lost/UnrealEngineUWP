// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FX/SlateRHIPostBufferProcessor.h"

#include "SlatePostBufferBlur.generated.h"

/**
 * Slate Post Buffer Processor that performs a simple gaussian blur to the backbuffer
 * 
 * Create a new asset deriving from this class to use / modify settings.
 */
UCLASS(Abstract, Blueprintable, CollapseCategories)
class SLATERHIRENDERER_API USlatePostBufferBlur : public USlateRHIPostBufferProcessor
{
	GENERATED_BODY()

public:

	UPROPERTY(interp, BlueprintReadWrite, Category = "GaussianBlur")
	float GaussianBlurStrength = 10;

public:

	virtual void PostProcess(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FSlateRHIRenderingPolicyInterface InRenderingPolicy, UTextureRenderTarget2D* InSlatePostBuffer);
};