// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ReachabilityAnalysisState.cpp: Incremental reachability analysis support.
=============================================================================*/

#include "UObject/ReachabilityAnalysisState.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/App.h"
#include "Misc/Fork.h"

namespace UE::GC
{

void FReachabilityAnalysisState::Init()
{
	NumIterations = 0;
}

void FReachabilityAnalysisState::SetupWorkers(int32 InNumWorkers)
{
	NumWorkers = InNumWorkers;
	Stats = {};
	bIsSuspended = false;
}

void FReachabilityAnalysisState::UpdateStats(const FProcessorStats& InStats)
{
	Stats.AddStats(InStats);
}

void FReachabilityAnalysisState::ResetWorkers()
{
	NumWorkers = 0;
}

void FReachabilityAnalysisState::FinishIteration()
{
	NumIterations++;
}

bool FReachabilityAnalysisState::CheckIfAnyContextIsSuspended()
{
	bIsSuspended = false;
	for (int32 WorkerIndex = 0; WorkerIndex < NumWorkers && !bIsSuspended; ++WorkerIndex)
	{
		bIsSuspended = Contexts[WorkerIndex]->bIsSuspended;
	}
	return bIsSuspended;
}

} // namespce UE::GC

