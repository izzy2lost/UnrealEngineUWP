// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComputeGraph.h"

#include "PCGModule.h"
#include "PCGNode.h"

#include "ComputeFramework/ComputeKernelCompileResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGComputeGraph)

void UPCGComputeGraph::OnKernelCompilationComplete(int32 InKernelIndex, FComputeKernelCompileResults const& InCompileResults)
{
	if (KernelToNode.IsValidIndex(InKernelIndex) && KernelToNode[InKernelIndex].Get())
	{
		KernelToCompileMessages.FindOrAdd(KernelToNode[InKernelIndex].Get()) = InCompileResults.Messages;
	}
	else
	{
		// We may in general have kernels with no corresponding node.
		UE_LOG(LogPCG, Verbose, TEXT("Compilation message ignored for kernel index %d which has no associated node."), InKernelIndex);
	}
}
