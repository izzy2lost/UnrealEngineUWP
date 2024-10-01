// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compute/Elements/PCGComputeGraphElement.h"

#include "PCGComponent.h"
#include "PCGManagedResource.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "Components/PCGProceduralISMComponent.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/StaticMesh.h"
#include "InstanceDataPackers/PCGInstanceDataPackerBase.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"

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
		TRACE_CPUPROFILER_EVENT_SCOPE(Context->bAllAsyncOperationsDone);

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
		UPCGDataBinding* DataBinding = FPCGContext::NewObject_AnyThread<UPCGDataBinding>(Context);
		Context->DataBinding.Reset(DataBinding);

		Context->DataBinding->Initialize(Graph.Get(), Context->SourceComponent, Context->InputData, Graph->GetAttributeLookupTable());

		// Perform validation after input data is initialized.
		for (TWeakObjectPtr<const UPCGNode> Node : Graph->KernelToNode)
		{
			const UPCGSettings* Settings = Node.Get() ? Node->GetSettings() : nullptr;

			if (!Settings || !Settings->IsKernelValid(Context, /*bQuiet=*/false))
			{
				return true;
			}
		}

		const bool bAnyComponentsSetup = SetupProceduralISMComponents(Context, DataBinding);

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ComputeGraphInstance.CreateDataProviders);
			Context->ComputeGraphInstance.CreateDataProviders(Graph.Get(), 0, Context->DataBinding.Get());
		}

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

		if (bAnyComponentsSetup)
		{
			// Delay to give time for proxy to settle and for scene update to allocate space in the GPU scene.
			SleepUntilNextFrame();
			return false;
		}
	}

	check(Context->DataBinding && InContext->SourceComponent.Get());

	// 1. Prepare render resources. In editor, this will trigger shader compile if not compiled already.
	if (!Graph->GetRenderProxy())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Graph->UpdateResources);

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
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ComputeGraphInstance.EnqueueWork);

		Context->bGraphEnqueued = Context->ComputeGraphInstance.EnqueueWork(
			Graph.Get(),
			InContext->SourceComponent->GetScene(),
			ComputeTaskExecutionGroup::EndOfFrameUpdate,
			InContext->SourceComponent->GetOwner()->GetFName(),
			FSimpleDelegate());
	}

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

bool FPCGComputeGraphElement::SetupProceduralISMComponents(FPCGContext* InContext, UPCGDataBinding* InBinding) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGComputeGraphElement::SetupProceduralISMComponents);
	check(InContext);
	check(InBinding);

	bool bAnyComponentsSetup = false;

	for (const UPCGSettings* Settings : Graph->StaticMeshSpawners)
	{
		const UPCGStaticMeshSpawnerSettings* SpawnerSettings = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
		if (!ensure(SpawnerSettings) || !SpawnerSettings->bEnabled)
		{
			continue;
		}

		AActor* TargetActor = SpawnerSettings->TargetActor.Get() ? SpawnerSettings->TargetActor.Get() : InContext->GetTargetActor(nullptr);
		if (!ensure(SpawnerSettings->MeshSelectorParameters) || !ensure(TargetActor))
		{
			continue;
		}

		const FPCGDataCollectionDesc InputDataDesc = SpawnerSettings->ComputeInputPinDataDesc(PCGPinConstants::DefaultInputLabel, InBinding);

		const uint32 InputPointCount = InputDataDesc.ComputeDataElementCount(EPCGDataType::Point);
		if (InputPointCount == 0)
		{
			continue;
		}

		const FBox LocalBounds = InContext->SourceComponent->GetGridBounds().ShiftBy(-InContext->SourceComponent->GetOwner()->GetActorLocation());

		uint32 CustomFloatCount = 0;

		TArray<FUint32Vector4> AttributeIdOffsetStrides;
		if (SpawnerSettings->InstanceDataPackerParameters && SpawnerSettings->InstanceDataPackerParameters->GetAttributeNames(/*OutNames=*/nullptr))
		{
			TArray<FName> AttributeNames;
			SpawnerSettings->InstanceDataPackerParameters->GetAttributeNames(&AttributeNames);

			PCGDataForGPUHelpers::ComputeCustomFloatPacking(AttributeNames, InBinding, InputDataDesc, CustomFloatCount, AttributeIdOffsetStrides);
		}

		TArray<float> PrimitiveSelectionCDF;
		int32 SelectorAttributeId = -1;
		TArray<int32> PrimitiveStringKeys;
		TArray<FPCGProceduralISMComponentDescriptor> ComponentsToCreate;

		if (const UPCGMeshSelectorByAttribute* SelectorByAttribute = Cast<UPCGMeshSelectorByAttribute>(SpawnerSettings->MeshSelectorParameters))
		{
			const FName SelectedName = SelectorByAttribute->AttributeName;
			if (SelectedName == NAME_None)
			{
				UE_LOG(LogPCG, Error, TEXT("Invalid mesh selector attribute specified '%s'."), *SelectedName.ToString());
				continue;
			}

			const FPCGKernelAttributeIDAndType* FoundAttribute = InBinding->GetAttributeLookupTable().Find(SelectedName);
			if (!FoundAttribute)
			{
				UE_LOG(LogPCG, Error, TEXT("Mesh selector attribute '%s' not found."), *SelectedName.ToString());
				continue;
			}

			SelectorAttributeId = FoundAttribute->Id;

			// Compute how many unique incoming values we will receive.
			TSet<int32> UniqueStringKeys;
			for (const FPCGDataDesc& Desc : InputDataDesc.DataDescs)
			{
				for (const FPCGKernelAttributeDesc& AttributeDesc : Desc.AttributeDescs)
				{
					if (AttributeDesc.Name == SelectedName)
					{
						UniqueStringKeys.Append(AttributeDesc.UniqueStringKeys);
					}
				}
			}

			for (int32 StringKey : UniqueStringKeys)
			{
				if (!ensure(InBinding->GetStringTable().IsValidIndex(StringKey)))
				{
					continue;
				}

				const FString& MeshPathString = InBinding->GetStringTable()[StringKey];
				
				FPCGProceduralISMComponentDescriptor Descriptor;
				Descriptor = SelectorByAttribute->TemplateDescriptor;
				Descriptor.NumInstances = InputPointCount;
				Descriptor.LocalBounds = LocalBounds;
				Descriptor.NumCustomFloats = CustomFloatCount;
				
				Descriptor.StaticMesh = Cast<UStaticMesh>(FSoftObjectPath(MeshPathString).TryLoad());
				if (!Descriptor.StaticMesh)
				{
					UE_LOG(LogPCG, Error, TEXT("Could not load static mesh from path '%s'."), *MeshPathString);
					continue;
				}

				PrimitiveStringKeys.Emplace(StringKey);
				ComponentsToCreate.Add(MoveTemp(Descriptor));
			}

			PrimitiveSelectionCDF.SetNumZeroed(ComponentsToCreate.Num());
		}
		else if (const UPCGMeshSelectorWeighted* SelectorWeighted = Cast<UPCGMeshSelectorWeighted>(SpawnerSettings->MeshSelectorParameters))
		{
			if (SelectorWeighted->MeshEntries.IsEmpty())
			{
				UE_LOG(LogPCG, Error, TEXT("No mesh entries provided."));
				continue;
			}

			float CumulativeWeight = 0.0f;

			float TotalWeight = 0.0f;
			for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
			{
				TotalWeight += Entry.Weight;
			}

			if (!ensure(TotalWeight > UE_SMALL_NUMBER))
			{
				continue;
			}

			PrimitiveSelectionCDF.Reserve(SelectorWeighted->MeshEntries.Num());

			for (const FPCGMeshSelectorWeightedEntry& Entry : SelectorWeighted->MeshEntries)
			{
				UStaticMesh* StaticMesh = Entry.Descriptor.StaticMesh.LoadSynchronous();

				const float Weight = float(Entry.Weight) / TotalWeight;
				CumulativeWeight += Weight;
				PrimitiveSelectionCDF.Add(CumulativeWeight);
				PrimitiveStringKeys.Add(InBinding->GetStringTable().IndexOfByKey(Entry.Descriptor.StaticMesh.ToString()));

				FPCGProceduralISMComponentDescriptor Descriptor;
				Descriptor = Entry.Descriptor;
				Descriptor.NumInstances = FMath::CeilToInt(InputPointCount * Weight);
				Descriptor.LocalBounds = LocalBounds;
				Descriptor.NumCustomFloats = CustomFloatCount;
				Descriptor.StaticMesh = StaticMesh;
				ComponentsToCreate.Add(MoveTemp(Descriptor));
			}
		}

		FPCGSpawnerPrimitives& Primitives = InBinding->MeshSpawnersToPrimitives.FindOrAdd(SpawnerSettings);
		Primitives.NumCustomFloats = CustomFloatCount;
		Primitives.AttributeIdOffsetStrides = MoveTemp(AttributeIdOffsetStrides);
		Primitives.SelectorAttributeId = SelectorAttributeId;
		Primitives.SelectionCDF = MoveTemp(PrimitiveSelectionCDF);
		Primitives.PrimitiveStringKeys = MoveTemp(PrimitiveStringKeys);

		for (const FPCGProceduralISMComponentDescriptor& Desc : ComponentsToCreate)
		{
			FPCGProceduralISMCBuilderParameters Params;
			Params.Descriptor = Desc;
			Params.bAllowDescriptorChanges = false;

			UPCGManagedProceduralISMComponent* MISMC = PCGManagedProceduralISMComponent::GetOrCreateManagedProceduralISMC(TargetActor, InContext->SourceComponent.Get(), SpawnerSettings->UID, Params);

			check(MISMC);
			MISMC->SetCrc(InContext->DependenciesCrc);

			// Don't bother registering the resource change as we're transient anyway.
			//InContext->TouchedResources.Emplace(MISMC);

			Primitives.Primitives.Add(MISMC->GetComponent());

			bAnyComponentsSetup = true;

			if (Primitives.Primitives.Num() >= PCGComputeConstants::MAX_PRIMITIVE_COMPONENTS_PER_SPAWNER)
			{
				UE_LOG(LogPCG, Warning, TEXT("Attempted to emit too many primitive components, terminated after creating %d."), Primitives.Primitives.Num());
				break;
			}
		}
	}

	return bAnyComponentsSetup;
}

void FPCGComputeGraphElement::PostExecuteInternal(FPCGContext* InContext) const
{
	check(InContext);
	FPCGComputeGraphContext* Context = static_cast<FPCGComputeGraphContext*>(InContext);

	if (!Context->DataBinding)
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
