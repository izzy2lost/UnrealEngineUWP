// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
 
struct PCGCOMPUTE_API FPCGTextureReadbackDispatchParams
{
	FTextureRHIRef SourceTexture;
	FSamplerStateRHIRef SourceSampler;
	FIntPoint SourceDimensions;
};
 
class PCGCOMPUTE_API FPCGTextureReadbackInterface
{
public:
	static void Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FPCGTextureReadbackDispatchParams& Params, const TFunction<void(void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)>& AsyncCallback);
	static void Dispatch_GameThread(const FPCGTextureReadbackDispatchParams& Params, const TFunction<void(void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)>& AsyncCallback);

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(const FPCGTextureReadbackDispatchParams& Params, const TFunction<void(void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)>& AsyncCallback);
};
