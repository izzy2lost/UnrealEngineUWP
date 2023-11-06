// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateRenderer.h"
#include "RHIFwd.h"
#include "UObject/Object.h"
#include "Interfaces/SlateRHIRenderingPolicyInterface.h"

#include "SlateRHIPostBufferProcessor.generated.h"

/**
 * Base class for types that can process the backbuffer scene into the slate post buffer.
 * 
 * Implement 'PostProcess' in your derived class. For an example see: USlatePostBufferBlur.
 */
UCLASS(Abstract, Blueprintable, CollapseCategories)
class SLATERHIRENDERER_API USlateRHIPostBufferProcessor : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Overridable postprocess for the given source scene backbuffer provided in 'Src' into 'Dst'
	 * You must override this method. In your override, you should copy params before executing 'ENQUEUE_RENDER_COMMAND'.
	 * This allows you to avoid render & game thread race conditions. See 'SlatePostBufferBlur' for example.
	 * Also avoid capturing [this] in your override, to avoid possible GC issues with the processor instance.
	 *
	 * @param InViewInfo			'FViewportInfo' resource used to get backbuffer in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used to get the 'BufferedRT' viewport texture used in PIE
	 * @param InElementWindowSize	Size of window being rendered, used to determine if using stereo rendering or not.
	 * @param InRenderingPolicy		Slate RHI RenderingPolicy
	 * @param InSlatePostBuffer		Texture render target used for final output
	 */
	virtual void PostProcess(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FSlateRHIRenderingPolicyInterface InRenderingPolicy, UTextureRenderTarget2D* InSlatePostBuffer)
	{
	}

protected:

	/** 
	 * Gets scene backbuffer, typically used as 'Src' texture for post process, but not always (Ex: PIE).
	 * 
	 * @param InViewInfo			'FViewportInfo' resource used to get backbuffer in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used to get the 'BufferedRT' viewport texture used in PIE
	 * @param InElementWindowSize	Size of window being rendered, used to determine if using stereo rendering or not.
	 * @param InRHICmdList			RHI command list to queue commands on
	 */
	static FTexture2DRHIRef GetBackbuffer_RenderThread(FRenderResource* InViewInfo, FRenderResource* InViewportTexture, FVector2D InElementWindowSize, FRHICommandListImmediate& InRHICmdList);

	/**
	 * Gets 'Src' texture for post process command. Typically the scenebuffer.
	 *
	 * @param InBackBuffer			Backbuffer used in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used for 'BufferedRT' viewport texture in PIE
	 */
	static FTexture2DRHIRef GetSrcTexture_RenderThread(FTexture2DRHIRef InBackBuffer, FRenderResource* InViewportTexture);

	/**
	 * Gets 'Dst' texture for post process command. Convience method, this should be possible through the direct resource.
	 *
	 * @param InSlatePostBuffer		Texture render target used for final output
	 */
	static FTextureReferenceRHIRef& GetDstTexture_RenderThread(UTextureRenderTarget2D* InSlatePostBuffer);

	/**
	 * Gets 'Dst' extent. Used for final size in post process command.
	 *
	 * @param InBackBuffer			Backbuffer used for size in standalone
	 * @param InViewportTexture		'FSlateRenderTargetRHI' resource used for 'BufferedRT' viewport texture size in PIE
	 */
	static FIntPoint GetDstExtent_RenderThread(FTexture2DRHIRef InBackBuffer, FRenderResource* InViewportTexture);
};