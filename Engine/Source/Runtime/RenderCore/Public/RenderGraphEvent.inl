// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if HAS_GPU_STATS
inline FRDGScope_GPU::FRDGScope_GPU(FRDGScopeState& State, FRHIGPUMask GPUMask, const FName& CsvStatName, const TStatId& Stat, const TCHAR* Description, FRHIDrawStatsCategory const& Category)
	: CurrentCategory(Category.ShouldCountDraws() ? &Category : nullptr)
{
	if (AreGPUStatsEnabled())
	{
		StartQuery = FRealtimeGPUProfiler::Get()->PushEvent(GPUMask, CsvStatName, Stat, Description);
	}
}

inline void FRDGScope_GPU::ImmediateEnd(FRDGScopeState&)
{
	if (StartQuery)
	{
		StopQuery = FRealtimeGPUProfiler::Get()->PopEvent();
	}
}

inline FRDGScope_GPU::~FRDGScope_GPU()
{
	if (StartQuery)
	{
		StartQuery.Discard(true);
	}
	if (StopQuery)
	{
		StopQuery.Discard(false);
	}
}

inline void FRDGScope_GPU::BeginCPU(FRHIComputeCommandList& RHICmdList, bool bPreScope)
{
	if (CurrentCategory)
	{
		PreviousCategory = RHICmdList.SetDrawStatsCategory(CurrentCategory);
	}
}

inline void FRDGScope_GPU::EndCPU(FRHIComputeCommandList& RHICmdList, bool bPreScope)
{
	if (CurrentCategory)
	{
		RHICmdList.SetDrawStatsCategory(PreviousCategory);
	}
}

inline void FRDGScope_GPU::BeginGPU(FRHIComputeCommandList& RHICmdList)
{
	if (StartQuery && EnumHasAnyFlags(RHICmdList.GetPipeline(), ERHIPipeline::Graphics))
	{
		StartQuery.Submit(static_cast<FRHICommandList&>(RHICmdList), true);
	}
}

inline void FRDGScope_GPU::EndGPU(FRHIComputeCommandList& RHICmdList)
{
	if (StopQuery && EnumHasAnyFlags(RHICmdList.GetPipeline(), ERHIPipeline::Graphics))
	{
		StopQuery.Submit(static_cast<FRHICommandList&>(RHICmdList), false);
	}
}
#endif

#if RDG_EVENTS

	inline FRDGScope_RHI::FRDGScope_RHI(FRDGScopeState& State, FRDGEventName&& Name)
		: Name(MoveTemp(Name))
	#if WITH_RHI_BREADCRUMBS
		, Node(Name.AllocBreadcrumb(State.GetBreadcrumbAllocator()))
	#endif
	{
	#if WITH_RHI_BREADCRUMBS
		if (Node)
		{
			Node->SetParent(State.CurrentBreadcrumbRef);
			State.CurrentBreadcrumbRef = Node;
			Node->BeginCPU();

			if (!State.ScopeState.bImmediate)
			{
				// Link breadcrumbs together, so we can iterate over them during RDG compilation.
				State.LocalBreadcrumbList.Append(Node);
			}
		}
	#endif
	}

	inline void FRDGScope_RHI::ImmediateEnd(FRDGScopeState& State)
	{
	#if WITH_RHI_BREADCRUMBS
		if (Node)
		{
			Node->EndCPU();
			State.CurrentBreadcrumbRef = Node->GetParent();
		}
	#endif
	}

#endif //  RDG_EVENTS

template <typename TScopeType>
template <typename... TArgs>
inline TRDGEventScopeGuard<TScopeType>::TRDGEventScopeGuard(FRDGScopeState& State, ERDGScopeFlags Flags, TArgs&&... Args)
	: State(State)
	, Scope(Allocate(State, Flags))
{
	if (Scope)
	{
		Scope->Impl.Emplace<TScopeType>(State, Forward<TArgs>(Args)...);

		if (State.ScopeState.bImmediate)
		{
			Scope->BeginCPU(State.RHICmdList, false);
			Scope->BeginGPU(State.RHICmdList);
		}
	}
}

template <typename TScopeType>
inline TRDGEventScopeGuard<TScopeType>::~TRDGEventScopeGuard()
{
	if (Scope)
	{
		if (State.ScopeState.bImmediate)
		{
			Scope->EndGPU(State.RHICmdList);
			Scope->EndCPU(State.RHICmdList, false);
		}

		Scope->ImmediateEnd(State);

		State.ScopeState.Mask &= ~(TypeMask);
		State.ScopeState.Current = State.ScopeState.Current->Parent;
	}
}

template <typename TScopeType>
inline FRDGScope* TRDGEventScopeGuard<TScopeType>::Allocate(FRDGScopeState& State, ERDGScopeFlags Flags)
{
	if (State.ScopeState.ScopeMode == ERDGScopeMode::Disabled && !EnumHasAnyFlags(Flags, ERDGScopeFlags::AlwaysEnable))
	{
		return nullptr;
	}

	if (State.ScopeState.ScopeMode == ERDGScopeMode::TopLevelOnly && (State.ScopeState.Mask & TypeMask))
	{
		return nullptr;
	}

	if (EnumHasAnyFlags(Flags, ERDGScopeFlags::Final))
	{
		// Mask off any nested scopes of the same type
		State.ScopeState.Mask |= TypeMask;
	}

	FRDGScope* Scope = FRDGAllocator::GetTLS().Alloc<FRDGScope>(State.ScopeState.Current);
	State.ScopeState.Current = Scope;

	return Scope;
}
