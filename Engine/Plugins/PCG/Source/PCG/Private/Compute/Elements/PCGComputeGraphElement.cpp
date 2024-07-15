// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGComputeGraphElement.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "Compute/DataInterfaces/PCGDataCollectionReadbackDataInterface.h"

#include "ComputeWorkerInterface.h"
#include "ComputeFramework/ComputeFramework.h"
#include "ComputeFramework/ComputeKernelCompileResult.h"
#include "GameFramework/Actor.h"
#include "Logging/LogVerbosity.h"

#define LOCTEXT_NAMESPACE "PCGComputeGraphElement"

void FPCGComputeGraphContext::AddExtraStructReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddPropertyReferences(FComputeGraphInstance::StaticStruct(), &ComputeGraphInstance);
}

#if WITH_EDITOR
bool FPCGComputeGraphElement::operator==(const FPCGComputeGraphElement& Other) const
{
	// Equivalence is same compute graph.
	// TODO: A compute graph is currently generated for every compile, so the presence of GPU nodes breaks the current
	// change detection. We could either cache compute graphs formed by subsets of GPU nodes that have not changed, or
	// we could do a deep equality check for compute graphs here.
	return Graph == Other.Graph;
}
#endif

bool FPCGComputeGraphElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGComputeGraphElement::ExecuteInternal);
	check(InContext);
	FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);

	if (!ensure(Graph))
	{
		return true;
	}

	// Drive the execution of a compute graph. The stages are commented below and numbered by the sequence in which they are executed.
	// The sequence appears out of order as calls so that paths that are executed multiple times (like checking for completion) are as
	// short as possible.

	// 7. Execution is is complete when any async readbacks are complete.
	if (Context->bAllAsyncOperationsDone)
	{
		Context->bExecutionSuccess = true;

		for (UComputeDataProvider* DataProvider : Context->ComputeGraphInstance.GetDataProviders())
		{
			if (UPCGDataProviderDataCollectionReadback* Readback = Cast<UPCGDataProviderDataCollectionReadback>(DataProvider))
			{
				// Process data for all readbacks, and track whether all succeeded.
				const bool bProcessResult = Readback->ProcessReadBackData();
				Context->bExecutionSuccess &= bProcessResult;
			}
		}

		// Currently we don't output anything if processing any readback data processing failed.
		if (ensure(Context->bExecutionSuccess) && ensure(Context->DataBinding))
		{
			Context->OutputData = Context->DataBinding->OutputDataCollection;
		}

		return true;
	}

	auto SleepUntilNextFrame = [Context]()
	{
		// TODO unsafe access to raw pointer, need cancellation lambda
		Context->bIsPaused = true;
		Context->SourceComponent->GetSubsystem()->RegisterBeginTickAction([Context]()
		{
			Context->bIsPaused = false;
		});
	};

	// 3. If still compiling, try again next frame.
	if (Graph->HasKernelResourcesPendingShaderCompilation())
	{
		UE_LOG(LogPCG, Log, TEXT("Deferring until next frame as the kernel has pending shader compilations."));
		SleepUntilNextFrame();
		return false;
	}

	// 6. Keep waiting for execution to complete.
	if (Context->bGraphEnqueued)
	{
		// Likely we need a frame to pass in order to make progress.
		SleepUntilNextFrame();
		return true;
	}

	// 4. Initialize and parse incoming data for data sizes, attributes, etc that will drive buffer allocations and dispatch thread counts.
	if (!Context->DataBinding)
	{
		UPCGDataBinding* DataBindingObject = FPCGContext::NewObject_AnyThread<UPCGDataBinding>(Context);
		Context->DataBinding.Reset(DataBindingObject);

		DataBindingObject->SourceComponent = Context->SourceComponent;
		DataBindingObject->Graph = Graph.Get();

		FPCGDataForGPU& DataForGPU = DataBindingObject->DataForGPU;

		DataForGPU.InputDataCollection = Context->InputData;

		// Link each input pin to the data collection, so that data providers can find the data.
		for (TWeakObjectPtr<const UPCGPin>& InputPinPtr : Graph->PinsReceivingDataFromCPU)
		{
			if (const UPCGPin* InputPin = InputPinPtr.Get())
			{
				DataForGPU.InputPins.Add(InputPin);
			}
		}

		DataForGPU.InputPinLabelAliases = Graph->InputPinLabelAliases;

		Context->ComputeGraphInstance.CreateDataProviders(Graph.Get(), 0, Context->DataBinding.Get());

		// Register all providers running async operations. TODO review if we should have a general API like "RunsAsyncOperations()"?
		for (UComputeDataProvider* DataProvider : Context->ComputeGraphInstance.GetDataProviders())
		{
			if (UPCGDataProviderDataCollectionReadback* Readback = Cast<UPCGDataProviderDataCollectionReadback>(DataProvider))
			{
				Context->ProvidersRunningAsyncOperations.Add(Readback);

				Readback->OnReadbackComplete_RenderThread().AddLambda([Context, Readback]()
				{
					FWriteScopeLock Lock(Context->ProvidersRunningAsyncOperationsLock);

					const bool bEmptyBefore = Context->ProvidersRunningAsyncOperations.IsEmpty();

					ensure(Context->ProvidersRunningAsyncOperations.Contains(Readback));
					Context->ProvidersRunningAsyncOperations.Remove(Readback);

					if (!bEmptyBefore && Context->ProvidersRunningAsyncOperations.IsEmpty())
					{
						Context->bAllAsyncOperationsDone = true;

						Context->bIsPaused = false;
					}
				});
			}
		}
	}

	check(Context->DataBinding && InContext->SourceComponent.Get());

	// 1. Prepare render resources. In editor, this will trigger shader compile if not compiled already.
	if (!Graph->GetRenderProxy())
	{
		Graph->UpdateResources();

		SleepUntilNextFrame();
		return false;
	}

	// 2. Validate compilation
	{
		// Add any messages that may have occurred during compilation to visual logs.
#if WITH_EDITOR
		LogCompilationMessages(Context);
#endif

		// If there was any error then we should abort.
		for (const TPair<TObjectKey<const UPCGNode>, TArray<FComputeKernelCompileMessage>>& NodeAndCompileMessages : Graph->KernelToCompileMessages)
		{
			for (const FComputeKernelCompileMessage& Message : NodeAndCompileMessages.Get<1>())
			{
				if (Message.Type == FComputeKernelCompileMessage::EMessageType::Error)
				{
					return true;
				}
			}
		}
	}

	// 5. Enqueue work to be executed when the GPU processes the current frame.
	Context->bGraphEnqueued = Context->ComputeGraphInstance.EnqueueWork(
		Graph.Get(),
		InContext->SourceComponent->GetScene(),
		ComputeTaskExecutionGroup::EndOfFrameUpdate,
		InContext->SourceComponent->GetOwner()->GetFName(),
		FSimpleDelegate());

	if (ensure(Context->bGraphEnqueued))
	{
		FReadScopeLock Lock(Context->ProvidersRunningAsyncOperationsLock);

		if (!Context->ProvidersRunningAsyncOperations.IsEmpty())
		{
			// If we're running async operations (like readbacks), go to sleep and let them wake us up later.
			Context->bIsPaused = true;
			return false;
		}
		else
		{
			// No operations to wait for, so signal completion.
			Context->bExecutionSuccess = true;
			return true;
		}
	}
	else
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("EnqueueFailed", "Compute graph enqueue failed, check log for errors."));
		ResetAsyncOperations(InContext);
		return true;
	}
}

void FPCGComputeGraphElement::PostExecuteInternal(FPCGContext* InContext) const
{
	check(InContext);
	FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);

	if (!ensure(Context->DataBinding))
	{
		return;
	}

#if WITH_EDITOR
	if (Context->bExecutionSuccess)
	{
		for (TWeakObjectPtr<const UPCGNode> NodePtr : Context->DataBinding->Graph->KernelToNode)
		{
			const UPCGNode* Node = NodePtr.Get();
			UPCGComponent* Component = Context->SourceComponent.Get();
			if (Component && Context->Stack && Node)
			{
				// TODO calling this to register that node executed. Regarding inspection data, we need to create
				// a readback and pipe the data to here.
				Component->StoreInspectionData(Context->Stack, Node, /*InTimer=*/nullptr, {}, {}, /*bUsedCache*/false);
			}
		}
	}
#endif
}

void FPCGComputeGraphElement::AbortInternal(FPCGContext* InContext) const
{
	ResetAsyncOperations(InContext);
}

void FPCGComputeGraphElement::ResetAsyncOperations(FPCGContext* InContext) const
{
	if (InContext)
	{
		FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);

		for (UComputeDataProvider* DataProvider : Context->ComputeGraphInstance.GetDataProviders())
		{
			if (UPCGDataProviderDataCollectionReadback* Readback = Cast<UPCGDataProviderDataCollectionReadback>(DataProvider))
			{
				Readback->OnReadbackComplete_RenderThread().Clear();
			}
		}

		FWriteScopeLock Lock(Context->ProvidersRunningAsyncOperationsLock);
		Context->ProvidersRunningAsyncOperations.Reset();
	}
}

#if WITH_EDITOR
void FPCGComputeGraphElement::LogCompilationMessages(FPCGComputeGraphContext* InContext) const
{
	if (InContext->SourceComponent.IsValid() && InContext->Stack)
	{
		for (const TPair<TObjectKey<const UPCGNode>, TArray<FComputeKernelCompileMessage>>& NodeAndCompileMessages : Graph->KernelToCompileMessages)
		{
			for (const FComputeKernelCompileMessage& Message : NodeAndCompileMessages.Get<1>())
			{
				// These messages already go to log. So just pick out the warnings and errors to display on graph. Need to convert
				// message type.
				ELogVerbosity::Type Verbosity = ELogVerbosity::All;
				if (Message.Type == FComputeKernelCompileMessage::EMessageType::Warning)
				{
					Verbosity = ELogVerbosity::Warning;
				}
				else if (Message.Type == FComputeKernelCompileMessage::EMessageType::Error)
				{
					Verbosity = ELogVerbosity::Error;
				}

				if (Verbosity < ELogVerbosity::Log)
				{
					if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(InContext->SourceComponent->GetWorld()))
					{
						FPCGStack StackWithNode = *InContext->Stack;
						StackWithNode.PushFrame(NodeAndCompileMessages.Get<0>().ResolveObjectPtr());

						Subsystem->GetNodeVisualLogsMutable().Log(StackWithNode, Verbosity, FText::FromString(Message.Text));
					}
				}
			}
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
