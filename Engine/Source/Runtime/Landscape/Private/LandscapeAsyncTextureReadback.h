// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RHIGPUReadback.h"
#include "RenderGraphFwd.h"

// Class that performs an async GPU texture readback for landscape purposes.
// Only supports BGRA8 texture format, and returns the results as an array of FColors.
class FLandscapeAsyncTextureReadback
{
private:
	// render thread state
	bool bAsyncReadbackSubmitOnRenderThread = false;
	bool bAsyncReadbackCompleteOnRenderThread = false;
	TUniquePtr<FRHIGPUTextureReadback> AsyncReadback;

	// results
	int32 TextureWidth = 0;
	int32 TextureHeight = 0;
	TArray<FColor> ReadbackResults;

	void FinishReadback_RenderThread();

public:
	// construct on the game thread
	FLandscapeAsyncTextureReadback() {}

	// destruct on the render thread.  From game thread, call QueueDeletion() instead
	~FLandscapeAsyncTextureReadback()
	{
		check(IsInRenderingThread());
	}

	// use this to start an async readback operation from the render thread (on a render graph texture)
	void StartReadback_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef RDGTexture);

	// TODO [chris.tchou] : add other StartReadback functions as needed

	// Check readback status and update the process if needed. Return true when the AsyncReadbackResults are available.
	// You must call this occasionally (from the game thread) or the readback may never complete.
	bool CheckAndUpdate();

	// Returns true when async readback results are available.  Call GetResults() to retrieve them.
	bool IsComplete()
	{
		return bAsyncReadbackCompleteOnRenderThread;
	}

	// Retrieve the async readback results.  Requires readback to be complete.
	// This function returns its internal memory buffer, relinquishing control over it, so this function can only be called once.
	TArray<FColor> TakeResults(FIntPoint* OutSize)
	{
		check(bAsyncReadbackCompleteOnRenderThread);
		if (OutSize)
		{
			*OutSize = FIntPoint(TextureWidth, TextureHeight);
		}
		return MoveTemp(ReadbackResults);
	}

	// once complete, call this to queue deletion of the readback object on the render thread
	// (this must be deleted on the render thread to avoid other render-thread queued commands from accessing a deallocated pointer)
	void QueueDeletionFromGameThread();
};

