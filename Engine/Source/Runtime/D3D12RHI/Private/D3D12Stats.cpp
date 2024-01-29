// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Stats.cpp:RHI Stats and timing implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "RHICoreStats.h"

void D3D12RHI::FD3DGPUProfiler::BeginFrame()
{
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GetEmitDrawEvents();
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--;
		}
		else
		{
			SetEmitDrawEvents(true);  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;

			CurrentEventNodeFrame = new FD3D12EventNodeFrame(GetParentDevice());
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	if (GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void D3D12RHI::FD3DGPUProfiler::EndFrame()
{
	if (GetEmitDrawEvents())
	{
		PopEvent();
		check(StackDepth == 0);
	}

	const uint32 GPUIndex = GetParentDevice()->GetGPUIndex();

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
		Parent->GetDefaultCommandContext().FlushCommands();
	}

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	check(!bTrackingEvents || CurrentEventNodeFrame);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			SetEmitDrawEvents(bOriginalGEmitDrawEvents);

			UE_LOG(LogD3D12RHI, Log, TEXT(""));
			UE_LOG(LogD3D12RHI, Log, TEXT(""));
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;

			// Only dump the event tree and generate the screenshot for the first GPU.  Eventually, we may want to collate
			// profiling data for all GPUs into a single tree, but the short term goal is to make profiling in the editor
			// functional at all with "-MaxGPUCount=2" (required to enable multiple GPUs for GPU Lightmass).  In the editor,
			// we don't actually render anything on the additional GPUs, but the editor's profile visualizer will pick up
			// whatever event tree we dumped last, which will be the empty one from the last GPU, making the results
			// useless without this code fix.  Unreal Insights would be preferred for multi-GPU profiling outside the editor.
			if (GPUIndex == 0)
			{
				CurrentEventNodeFrame->DumpEventTree();

				if (RHIConfig::ShouldSaveScreenshotAfterProfilingGPU()
					&& GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec(NULL, TEXT("SCREENSHOT"), *GLog);
				}
			}
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		//@todo this really detects any hitch, even one on the game thread.
		// it would be nice to restrict the test to stalls on D3D, but for now...
		// this needs to be out here because bTrackingEvents is false during the hitch debounce
		static double LastTime = -1.0;
		double Now = FPlatformTime::Seconds();
		if (bTrackingEvents)
		{
			/** How long, in seconds a frame much be to be considered a hitch **/
			const float HitchThreshold = RHIConfig::GetGPUHitchThreshold();
			float ThisTime = Now - LastTime;
			bool bHitched = (ThisTime > HitchThreshold) && LastTime > 0.0 && CurrentEventNodeFrame;
			if (bHitched)
			{
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** Hitch detected on CPU, frametime = %6.1fms"), ThisTime * 1000.0f);
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));

				for (int32 Frame = 0; Frame < GPUHitchEventNodeFrames.Num(); Frame++)
				{
					UE_LOG(LogD3D12RHI, Warning, TEXT(""));
					UE_LOG(LogD3D12RHI, Warning, TEXT(""));
					UE_LOG(LogD3D12RHI, Warning, TEXT("********** GPU Frame: Current - %d"), GPUHitchEventNodeFrames.Num() - Frame);
					GPUHitchEventNodeFrames[Frame].DumpEventTree();
				}
				UE_LOG(LogD3D12RHI, Warning, TEXT(""));
				UE_LOG(LogD3D12RHI, Warning, TEXT(""));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** GPU Frame: Current"));
				CurrentEventNodeFrame->DumpEventTree();

				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("********** End Hitch GPU Profile"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("*******************************************************************************"));
				if (GEngine->GameViewport)
				{
					GEngine->GameViewport->Exec(NULL, TEXT("SCREENSHOT"), *GLog);
				}

				GPUHitchDebounce = 5; // don't trigger this again for a while
				GPUHitchEventNodeFrames.Empty(); // clear history
			}
			else if (CurrentEventNodeFrame) // this will be null for discarded frames while recovering from a recent hitch
			{
				/** How many old frames to buffer for hitch reports **/
				static const int32 HitchHistorySize = 4;

				if (GPUHitchEventNodeFrames.Num() >= HitchHistorySize)
				{
					GPUHitchEventNodeFrames.RemoveAt(0);
				}
				GPUHitchEventNodeFrames.Add((FD3D12EventNodeFrame*)CurrentEventNodeFrame);
				CurrentEventNodeFrame = NULL;  // prevent deletion of this below; ke kept it in the history
			}
		}
		LastTime = Now;
	}
	bTrackingEvents = false;
	delete CurrentEventNodeFrame;
	CurrentEventNodeFrame = NULL;
}

/** Start this frame of per tracking */
void FD3D12EventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FD3D12EventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FD3D12EventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming();
		const uint64 GPUFreq = RootEventTiming.GetTimingFrequency(GetParentDevice()->GetGPUIndex());

		RootResult = double(GPUTiming) / double(GPUFreq);
	}

	return (float)RootResult;
}

float FD3D12EventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		// Get the timing result and block the CPU until it is ready
		const uint64 GPUTiming = Timing.GetTiming();
		const uint64 GPUFreq = Timing.GetTimingFrequency(GetParentDevice()->GetGPUIndex());

		Result = double(GPUTiming) / double(GPUFreq);
	}

	return Result;
}

static TStatId GetD3D12BufferStat(const FRHIBufferDesc& BufferDesc)
{
	if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::UnorderedAccess))
	{
		return GET_STATID(STAT_D3D12UAVBuffers);
	}
	
	if (EnumHasAnyFlags(BufferDesc.Usage, EBufferUsageFlags::AccelerationStructure))
	{
		return GET_STATID(STAT_D3D12RTBuffers);
	}

	return GET_STATID(STAT_D3D12Buffer);
}

void D3D12BufferStats::UpdateBufferStats(FD3D12Buffer& Buffer, bool bAllocating)
{
	const FRHIBufferDesc& BufferDesc = Buffer.GetDesc();
	const FD3D12ResourceLocation& Location = Buffer.ResourceLocation;

	const int64 BufferSize = Location.GetSize();
	const int64 RequestedSize = bAllocating ? BufferSize : -BufferSize;

	UE::RHICore::UpdateGlobalBufferStats(BufferDesc, RequestedSize);

	INC_MEMORY_STAT_BY_FName(GetD3D12BufferStat(BufferDesc).GetName(), RequestedSize);
	INC_MEMORY_STAT_BY(STAT_D3D12MemoryCurrentTotal, RequestedSize);

#if UE_MEMORY_TRACE_ENABLED
	const D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = Location.GetGPUVirtualAddress();

	if (bAllocating)
	{
		// Skip if it's created as a
		// 1) standalone resource, because MemoryTrace_Alloc has been called in FD3D12Adapter::CreateCommittedResource
		// 2) placed resource from a pool allocator, because MemoryTrace_Alloc has been called in FD3D12Adapter::CreatePlacedResource
		if (!Location.IsStandaloneOrPooledPlacedResource())
		{
			MemoryTrace_Alloc(GPUAddress, BufferSize, Buffer.BufferAlignment, EMemoryTraceRootHeap::VideoMemory);
		}
	}
	else
	{
		MemoryTrace_Free(GPUAddress, EMemoryTraceRootHeap::VideoMemory);
	}
#endif
}
