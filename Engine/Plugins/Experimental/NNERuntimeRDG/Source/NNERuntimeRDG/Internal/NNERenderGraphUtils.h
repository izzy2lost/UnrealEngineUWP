// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNERuntimeCPU.h"
#include "NNERuntimeRDG.h"
#include "RenderGraph.h"

#if UE_TRACE_ENABLED
	#define NNE_TRACE_EVENT_SCOPED(Name) SCOPED_NAMED_EVENT_TEXT(#Name, FColor::Green)
#else
	#define NNE_TRACE_EVENT_SCOPED(Name)
#endif

namespace UE::NNE::Internal
{

/*
* Utility class to manage GPU -> CPU readbacks
*/
class FReadbackManager
{
	struct FReadback
	{
		FStagingBufferRHIRef	StagingBuffer;
		FGPUFenceRHIRef			Fence;
		FRDGBufferRef			Buffer;
		void*					DstData;
		uint32					NumBytes;
	};

	BEGIN_SHADER_PARAMETER_STRUCT(FReadbackPassParameters, )
		RDG_BUFFER_ACCESS_ARRAY(ReadbackBuffers)
	END_SHADER_PARAMETER_STRUCT()

public:

	FReadbackManager()
	{
		check(IsInGameThread());
		Signal = FGenericPlatformProcess::GetSynchEventFromPool(true);
	}

	~FReadbackManager()
	{
		check(IsInGameThread());
		FGenericPlatformProcess::ReturnSynchEventToPool(Signal);
		Signal = nullptr;
	}

	// Note: call on the game thread
	bool Init(int32 InNumReadbacks)
	{
		NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_Init);

		check(IsInGameThread());
		MaxNumReadbacks = InNumReadbacks;
		Signal->Reset();

		return true;
	}

	void BeginReadbacks_RenderThread(FRDGBuilder& RDGBuilder)
	{
		NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_BeginReadbacks);

		check(IsInRenderingThread());
		check(Readbacks.IsEmpty());
		check(MaxNumReadbacks > 0);

		NumAddedReadbacks = 0;
		NumProcessedReadbacks = 0;
		Readbacks.SetNum(MaxNumReadbacks);
		
		ReadbackParams = RDGBuilder.AllocParameters<FReadbackPassParameters>();

		for (FReadback& Readback : Readbacks)
		{
			Readback.StagingBuffer = RHICreateStagingBuffer();
			Readback.StagingBuffer->DisableLifetimeExtension();

			Readback.Fence = RHICreateGPUFence(TEXT("FReadbackManager_ReadbackFence"));
			Readback.Fence->DisableLifetimeExtension();
			Readback.Fence->Clear();

			Readback.Buffer = nullptr;
			Readback.DstData = nullptr;
			Readback.NumBytes = 0;
		}
	}

	void AddReadbacks_RenderThread(TConstArrayView<FTensorBindingRDG> InBindingsRDG, TConstArrayView<FTensorBindingCPU> InBindingsCPU)
	{
		NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_AddReadbacks_RT);

		check(IsInRenderingThread());
		check(InBindingsRDG.Num() == InBindingsCPU.Num());
		check(InBindingsRDG.Num() <= MaxNumReadbacks);

		if (InBindingsRDG.Num() != InBindingsCPU.Num())
		{
			UE_LOG(LogNNE, Error, TEXT("FReadbackManager:Number of bindings need to be same"));
			return;
		}

		if (InBindingsRDG.Num() > MaxNumReadbacks)
		{
			UE_LOG(LogNNE, Error, TEXT("FReadbackManager:Number of bindings is larger than the maximum number of readbacks provided by the Init()"));
			return;
		}

		for (int32 Idx = 0; Idx < InBindingsRDG.Num(); ++Idx)
		{
			ReadbackParams->ReadbackBuffers.Emplace(InBindingsRDG[Idx].Buffer, ERHIAccess::CopySrc);

			FReadback& Readback = Readbacks[NumAddedReadbacks];
			Readback.Buffer = InBindingsRDG[Idx].Buffer;
			Readback.DstData = InBindingsCPU[Idx].Data;
			Readback.NumBytes = InBindingsCPU[Idx].SizeInBytes;

			NumAddedReadbacks++;
		}
	}

	void EndReadbacks_RenderThread(FRDGBuilder& RDGBuilder)
	{
		check(IsInRenderingThread());

		RDGBuilder.AddPass(
			RDG_EVENT_NAME("FReadbackManager_ProcessReadbacks"),
			ReadbackParams,
			ERDGPassFlags::Readback | ERDGPassFlags::NeverCull,
			[this](FRHICommandListImmediate& RHICmdList)
			{
				NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_EndReadbacks_RT);
				
				for (FReadback& Readback : Readbacks)
				{
					Readback.Buffer->MarkResourceAsUsed();

					RHICmdList.CopyToStagingBuffer(Readback.Buffer->GetRHI(), Readback.StagingBuffer, 0, Readback.NumBytes);					
					RHICmdList.WriteGPUFence(Readback.Fence);
				}

				RHICmdList.BlockUntilGPUIdle();

				for (FReadback& Readback : Readbacks)
				{
					const void* SrcData = RHICmdList.LockStagingBuffer(Readback.StagingBuffer, Readback.Fence.GetReference(), 0, Readback.NumBytes);
					check(SrcData);

					if (SrcData)
					{
						FMemory::Memcpy(Readback.DstData, SrcData, Readback.NumBytes);
						RHICmdList.UnlockStagingBuffer(Readback.StagingBuffer);

						Readback.StagingBuffer = nullptr;
						Readback.Fence = nullptr;
					}

					++NumProcessedReadbacks;
				}
				
				// Clean-up resources (staging buffers, fences)
				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

				Signal->Trigger();
			}
		);
	}

	// Note: call on the game thread
	void Wait()
	{
		check(IsInGameThread());

		NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_Wait);

		check(MaxNumReadbacks > 0);
		Signal->Wait();

		while (NumProcessedReadbacks < MaxNumReadbacks)
		{
			FPlatformProcess::Sleep(0.f);
		}

		NumAddedReadbacks = 0;
		NumProcessedReadbacks = 0;
		MaxNumReadbacks = 0;
		Readbacks.Empty();
	}


private:

	using ReadbackArray = TArray<FReadback, TInlineAllocator<16>>;
	
	ReadbackArray				Readbacks;
	FReadbackPassParameters*	ReadbackParams = nullptr;
	std::atomic<int32>			MaxNumReadbacks;
	std::atomic<int32>			NumAddedReadbacks;
	std::atomic<int32>			NumProcessedReadbacks;
	FEvent*						Signal = nullptr;
};

} // namespace UE::NNE
