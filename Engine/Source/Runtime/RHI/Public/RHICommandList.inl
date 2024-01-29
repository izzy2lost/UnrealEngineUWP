// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandList.inl: RHI Command List inline definitions.
=============================================================================*/

#pragma once

class FRHICommandListBase;
class FRHICommandListExecutor;
class FRHICommandListImmediate;
class FRHIResource;
class FScopedRHIThreadStaller;
struct FRHICommandBase;

FORCEINLINE_DEBUGGABLE bool FRHICommandListBase::IsImmediate() const
{
	return PersistentState.bImmediate;
}

FORCEINLINE_DEBUGGABLE FRHICommandListImmediate& FRHICommandListBase::GetAsImmediate()
{
	checkf(IsImmediate(), TEXT("This operation expects the immediate command list."));
	return static_cast<FRHICommandListImmediate&>(*this);
}

FORCEINLINE_DEBUGGABLE bool FRHICommandListBase::Bypass() const
{
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
	return GRHICommandList.Bypass() && IsImmediate();
#else
	return false;
#endif
}

FORCEINLINE_DEBUGGABLE FScopedRHIThreadStaller::FScopedRHIThreadStaller(class FRHICommandListImmediate& InImmed, bool bDoStall)
	: Immed(nullptr)
{
	if (bDoStall && IsRunningRHIInSeparateThread())
	{
		check(IsInRenderingThread());
		if (InImmed.StallRHIThread())
		{
			Immed = &InImmed;
		}
	}
}

FORCEINLINE_DEBUGGABLE FScopedRHIThreadStaller::~FScopedRHIThreadStaller()
{
	if (Immed)
	{
		Immed->UnStallRHIThread();
	}
}

namespace PipelineStateCache
{
	/* Evicts unused state entries based on r.pso.evictiontime time. Called in RHICommandList::BeginFrame */
	extern RHI_API void FlushResources();
}

inline void FRHIComputeCommandList::SubmitCommandsHint()
{
	if (IsImmediate())
	{
		static_cast<FRHICommandListImmediate&>(*this).SubmitCommandsHint();
	}
}

// Helper class for traversing a FRHICommandList
class FRHICommandListIterator
{
public:
	FRHICommandListIterator(FRHICommandListBase& CmdList)
	{
		CmdPtr = CmdList.Root;
#if DO_CHECK
		NumCommands = 0;
		CmdListNumCommands = CmdList.NumCommands;
#endif
	}
	~FRHICommandListIterator()
	{
#if DO_CHECK
		checkf(CmdListNumCommands == NumCommands, TEXT("Missed %d Commands!"), CmdListNumCommands - NumCommands);
#endif
	}

	FORCEINLINE_DEBUGGABLE bool HasCommandsLeft() const
	{
		return !!CmdPtr;
	}

	FORCEINLINE_DEBUGGABLE FRHICommandBase* NextCommand()
	{
		FRHICommandBase* RHICmd = CmdPtr;
		CmdPtr = RHICmd->Next;
#if DO_CHECK
		NumCommands++;
#endif
		return RHICmd;
	}

private:
	FRHICommandBase* CmdPtr;

#if DO_CHECK
	uint32 NumCommands;
	uint32 CmdListNumCommands;
#endif
};

inline FRHICommandListScopedPipelineGuard::FRHICommandListScopedPipelineGuard(FRHICommandListBase& RHICmdList)
	: RHICmdList(RHICmdList)
{
	if (RHICmdList.GetPipeline() == ERHIPipeline::None)
	{
		RHICmdList.SwitchPipeline(ERHIPipeline::Graphics);
		bPipelineSet = true;
	}
}

inline FRHICommandListScopedPipelineGuard::~FRHICommandListScopedPipelineGuard()
{
	if (bPipelineSet)
	{
		RHICmdList.SwitchPipeline(ERHIPipeline::None);
	}
}

#if WITH_RHI_BREADCRUMBS

	template<size_t N, typename... TArgs>
	inline FRHIBreadcrumbEventScope::FRHIBreadcrumbEventScope(FRHIComputeCommandList& RHICmdList, bool bCondition, TCHAR const(&FormatString)[N], TArgs&&... Args)
		: RHICmdList(&RHICmdList)
		, Node(bCondition ? RHICmdList.GetBreadcrumbAllocator().AllocBreadcrumb(FormatString, Forward<TArgs>(Args)...) : nullptr)
	#if DO_CHECK
		, Pipeline(RHICmdList.GetPipeline())
	#endif
	{
		if (Node)
		{
			Node->SetParent(RHICmdList.PersistentState.LocalBreadcrumb);
			RHICmdList.BeginBreadcrumbCPU(Node, true);
			RHICmdList.BeginBreadcrumbGPU(Node);
		}
	}

	template<size_t N, typename... TArgs>
	inline FRHIBreadcrumbEventScope::FRHIBreadcrumbEventScope(IRHIComputeContext& RHIContext, bool bCondition, TCHAR const(&FormatString)[N], TArgs&&... Args)
		: FRHIBreadcrumbEventScope(FRHIComputeCommandList::Get(RHIContext.GetExecutingCommandList()), bCondition, FormatString, Forward<TArgs>(Args)...)
	{}

	inline FRHIBreadcrumbEventScope::~FRHIBreadcrumbEventScope()
	{
		checkf(Pipeline == RHICmdList->GetPipeline(), TEXT("Breadcrumb event was started and ended on different pipelines. Start: %s, End: %s")
			, *GetRHIPipelineName(Pipeline)
			, *GetRHIPipelineName(RHICmdList->GetPipeline())
		);

		if (Node)
		{
			RHICmdList->EndBreadcrumbGPU(Node);
			RHICmdList->EndBreadcrumbCPU(Node, true);
		}
	}

	template<size_t N, typename... TArgs>
	inline FRHIBreadcrumbEventManual::FRHIBreadcrumbEventManual(FRHIComputeCommandList& RHICmdList, TCHAR const(&FormatString)[N], TArgs&&... Args)
		: Node(RHICmdList.GetBreadcrumbAllocator().AllocBreadcrumb(FormatString, Forward<TArgs>(Args)...))
	#if DO_CHECK
		, Pipeline(RHICmdList.GetPipeline())
		, ThreadId(FPlatformTLS::GetCurrentThreadId())
	#endif
	{
		check(Pipeline != ERHIPipeline::None);

		Node->SetParent(RHICmdList.PersistentState.LocalBreadcrumb);
		RHICmdList.BeginBreadcrumbCPU(Node.Get(), true);
		RHICmdList.BeginBreadcrumbGPU(Node.Get());
	}

	inline void FRHIBreadcrumbEventManual::End(FRHIComputeCommandList& RHICmdList)
	{
		checkf(Node, TEXT("Manual breadcrumb was already ended."));
		checkf(Pipeline == RHICmdList.GetPipeline(), TEXT("Manual breadcrumb was started and ended on different pipelines. Start: %s, End: %s")
			, *GetRHIPipelineName(Pipeline)
			, *GetRHIPipelineName(RHICmdList.GetPipeline())
		);

		checkf(ThreadId == FPlatformTLS::GetCurrentThreadId(), TEXT("Manual breadcrumbs must be started and ended on the same thread."));

		RHICmdList.EndBreadcrumbGPU(Node.Get());
		RHICmdList.EndBreadcrumbCPU(Node.Get(), true);
		Node = {};
	}

	inline FRHIBreadcrumbEventManual::~FRHIBreadcrumbEventManual()
	{
		checkf(!Node, TEXT("Manual breadcrumb was destructed before it was ended."));
	}

#endif // WITH_RHI_BREADCRUMBS
