// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGComputeGraphElement.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"

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

	// 7. Execution is complete when any async readbacks are complete.
	if (Context->bAllAsyncOperationsDone)
	{
		Context->bExecutionSuccess = true;

		for (UComputeDataProvider* DataProvider : Context->ComputeGraphInstance.GetDataProviders())
		{
			UPCGDataCollectionDataProvider* PCGDataProvider = Cast<UPCGDataCollectionDataProvider>(DataProvider);
			if (PCGDataProvider && PCGDataProvider->RequiresReadback())
			{
				// Process data for all readbacks, and track whether all succeeded.
				const bool bProcessResult = PCGDataProvider->ProcessReadBackData(Context);
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
			UPCGDataCollectionDataProvider* PCGDataProvider = Cast<UPCGDataCollectionDataProvider>(DataProvider);
			if (PCGDataProvider && PCGDataProvider->RequiresReadback())
			{
				Context->ProvidersRunningAsyncOperations.Add(PCGDataProvider);

				PCGDataProvider->OnReadbackComplete_RenderThread().AddLambda([Context, PCGDataProvider]()
				{
					FWriteScopeLock Lock(Context->ProvidersRunningAsyncOperationsLock);

					const bool bEmptyBefore = Context->ProvidersRunningAsyncOperations.IsEmpty();

					ensure(Context->ProvidersRunningAsyncOperations.Contains(PCGDataProvider));
					Context->ProvidersRunningAsyncOperations.Remove(PCGDataProvider);

					if (!bEmptyBefore && Context->ProvidersRunningAsyncOperations.IsEmpty())
					{
						Context->bAllAsyncOperationsDone = true;

						Context->bIsPaused = false;
					}
				});
			}
		}

		for (TWeakObjectPtr<const UPCGNode> Node : Graph->KernelToNode)
		{
			const UPCGSettings* Settings = Node.Get() ? Node->GetSettings() : nullptr;

			if (Settings)
			{
				const UPCGCustomHLSLSettings* KernelSettings = CastChecked<UPCGCustomHLSLSettings>(Settings);

				if (!KernelSettings->IsKernelValid(Context, /*bQuiet=*/false))
				{
					return true;
				}
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
				// Some error messages were getting lost, and we were only getting the final 'failed' message. Treat this as failure and report for now.
				// TODO: Revert the 'failed' part once we're happy all relevant issues are bubbling up.
				if (Message.Type == FComputeKernelCompileMessage::EMessageType::Error || Message.Text.Contains(TEXT("failed"), ESearchCase::IgnoreCase))
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
				Component->NotifyNodeExecuted(Node, Context->Stack, /*InTimer=*/nullptr, /*bUsedCache*/false);
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
			UPCGDataCollectionDataProvider* PCGDataProvider = Cast<UPCGDataCollectionDataProvider>(DataProvider);
			if (PCGDataProvider && PCGDataProvider->RequiresReadback())
			{
				PCGDataProvider->OnReadbackComplete_RenderThread().Clear();
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
				else if (Message.Text.Contains(TEXT("failed"), ESearchCase::IgnoreCase))
				{
					// Some error messages were getting lost, and we were only getting the final 'failed' message.
					// Treat this as failure and report for now.
					// TODO: Revert this once we're happy all relevant issues are bubbling up.
					Verbosity = ELogVerbosity::Error;
				}

				if (Verbosity < ELogVerbosity::Log)
				{
					if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(InContext->SourceComponent->GetWorld()))
					{
						FPCGStack StackWithNode = *InContext->Stack;
						StackWithNode.PushFrame(NodeAndCompileMessages.Get<0>().ResolveObjectPtr());

						FText LogText;

						if (Message.Line != INDEX_NONE)
						{
							if (Message.ColumnStart != INDEX_NONE)
							{
								LogText = FText::Format(LOCTEXT("ErrorWithLineColFormat", "[{0},{1}] {2}"), Message.Line, Message.ColumnStart, FText::FromString(Message.Text));
							}
							else
							{
								LogText = FText::Format(LOCTEXT("ErrorWithLineFormat", "[{0}] {1}"), Message.Line, FText::FromString(Message.Text));
							}
						}
						else
						{
							LogText = FText::FromString(Message.Text);
						}

						Subsystem->GetNodeVisualLogsMutable().Log(StackWithNode, Verbosity, LogText);
					}
				}
			}
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE
