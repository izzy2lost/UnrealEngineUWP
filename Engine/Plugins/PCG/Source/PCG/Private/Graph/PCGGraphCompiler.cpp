// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGGraphCompiler.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGInputOutputSettings.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeKernelSource.h"
#include "Compute/DataInterfaces/PCGComputeDataInterface.h"
#include "Compute/DataInterfaces/PCGCustomKernelDataInterface.h"
#include "Compute/DataInterfaces/PCGDataCollectionDataInterface.h"
#include "Compute/DataInterfaces/PCGDataCollectionReadbackDataInterface.h"
#include "Compute/DataInterfaces/PCGDataCollectionUploadDataInterface.h"
#include "Compute/DataInterfaces/PCGDebugDataInterface.h"
#include "Compute/DataInterfaces/PCGLandscapeDataInterface.h"
#include "Compute/DataInterfaces/PCGTextureDataInterface.h"
#include "Compute/Elements/PCGComputeGraphElement.h"
#include "Compute/Elements/PCGCustomHLSL.h"
#include "Elements/PCGGather.h"
#include "Elements/PCGHiGenGridSize.h"
#include "Elements/PCGReroute.h"
#include "Graph/PCGGraphExecutor.h"
#include "Graph/PCGPinDependencyExpression.h"

#include "ComputeFramework/ComputeKernel.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeRWLock.h"
#include "Shader/ShaderTypes.h"

namespace PCGGraphCompiler
{
	TAutoConsoleVariable<bool> CVarEnableTaskStaticCulling(
		TEXT("pcg.GraphExecution.TaskStaticCulling"),
		true,
		TEXT("Enable static culling of tasks which considers static branches, generation grid size, trivial nodes and more."));

	TAutoConsoleVariable<bool> CVarEnableGPUExecution(
		TEXT("pcg.GraphExecution.GPU.Enable"),
		true,
		TEXT("Whether to emit compatible nodes as compute graphs to execute on the GPU."));

#if WITH_EDITOR
	TAutoConsoleVariable<bool> CVarEnableGPUDebugging(
		TEXT("pcg.GraphExecution.GPU.EnableDebugging"),
		false,
		TEXT("Enable verbose logging of GPU compilation and execution."));
#endif
}

TArray<FPCGGraphTask> FPCGGraphCompiler::CompileGraph(UPCGGraph* InGraph, FPCGTaskId& NextId, FPCGStackContext& InOutStackContext)
{
	if (!InGraph)
	{
		return TArray<FPCGGraphTask>();
	}

	InOutStackContext.PushFrame(InGraph);

	TArray<FPCGGraphTask> CompiledTasks;
	TMap<const UPCGNode*, FPCGTaskId> IdMapping;
	TArray<const UPCGNode*> NodeQueue;

	// Prime the node queue with all nodes that have no inbound edges
	for (const UPCGNode* Node : InGraph->GetNodes())
	{
		if(!Node->HasInboundEdges())
		{
			NodeQueue.Add(Node);
		}
	}

	// By definition, the input node has no inbound edge.
	// Put it last in the queue so it gets picked up first - order is important for hooking up the fetch input element
	NodeQueue.Add(InGraph->GetInputNode());

	while (NodeQueue.Num() > 0)
	{
		const UPCGNode* Node = NodeQueue.Pop();

		const UPCGBaseSubgraphNode* SubgraphNode = Cast<const UPCGBaseSubgraphNode>(Node);
		UPCGGraph* Subgraph = SubgraphNode ? SubgraphNode->GetSubgraph() : nullptr;
		const UPCGBaseSubgraphSettings* SubgraphSettings = SubgraphNode ? Cast<const UPCGBaseSubgraphSettings>(SubgraphNode->GetSettings()) : nullptr;

		// Note that recursive graphs must be dynamic by definition, as we aren't able to 'finish' emitting compiled tasks during compilation otherwise.
		// Implementation note: it is very important that the predicate here is symmetrical with the one in the subgraph node otherwise some tasks could be missing.
		const bool bIsRecursiveGraph = Subgraph && Subgraph->Contains(InGraph);
		const bool bIsNonDynamic = SubgraphSettings && !SubgraphSettings->IsDynamicGraph();
		const bool bIsNotDisabled = SubgraphSettings && SubgraphSettings->bEnabled;

		if(Subgraph && bIsNonDynamic && bIsNotDisabled && !bIsRecursiveGraph)
		{
			const FPCGTaskId PreId = NextId++;

			// 1. Compile the subgraph making sure we don't reuse the same ids
			// Note that we will not consume the pre or post-execute tasks, ergo bIsTopGraph=false
			FPCGStackContext SubgraphStackContext;
			// Passed uninitialized grid size to get all tasks
			TArray<FPCGGraphTask> Subtasks = GetCompiledTasks(Subgraph, PCGHiGenGrid::UninitializedGridSize(), SubgraphStackContext, /*bIsTopGraph=*/false);

#if WITH_EDITOR
			GraphDependenciesLock.Lock();
			GraphDependencies.AddUnique(Subgraph, InGraph);
			GraphDependenciesLock.Unlock();
#endif // WITH_EDITOR

			// Append all the stack frames from inside the subgraph to my current graph stack
			InOutStackContext.PushFrame(Node);
			const int32 StackOffset = InOutStackContext.GetNumStacks();
			for (FPCGGraphTask& Subtask : Subtasks)
			{
				Subtask.StackIndex += StackOffset;
			}
			InOutStackContext.AppendStacks(SubgraphStackContext);
			InOutStackContext.PopFrame();

			OffsetNodeIds(Subtasks, NextId, PreId);
			NextId += Subtasks.Num();

			const UPCGNode* SubgraphInputNode = Subgraph->GetInputNode();
			const UPCGNode* SubgraphOutputNode = Subgraph->GetOutputNode();

			// 2. Update the "input" and "output" node tasks so we can add the proper dependencies
			FPCGGraphTask* InputNodeTask = Subtasks.FindByPredicate([SubgraphInputNode](const FPCGGraphTask& Subtask) {
				return Subtask.Node == SubgraphInputNode;
				});

			FPCGGraphTask* OutputNodeTask = Subtasks.FindByPredicate([SubgraphOutputNode](const FPCGGraphTask& Subtask) {
				return Subtask.Node == SubgraphOutputNode;
				});

			// Build pre-task
			FPCGGraphTask& PreTask = CompiledTasks.Emplace_GetRef();
			PreTask.Node = Node;
			PreTask.NodeId = PreId;
			PreTask.StackIndex = InOutStackContext.GetCurrentStackIndex();

			for (const UPCGPin* InputPin : Node->InputPins)
			{
				check(InputPin);
				for (const UPCGEdge* InboundEdge : InputPin->Edges)
				{
					if (InboundEdge->IsValid())
					{
						PreTask.Inputs.Emplace(IdMapping[InboundEdge->InputPin->Node], InboundEdge->InputPin->Properties, InboundEdge->OutputPin->Properties);
					}
					else
					{
						UE_LOG(LogPCG, Warning, TEXT("Invalid inbound edge on subgraph"));
					}
				}
			}

			// Add pre-task as input to subgraph input node task
			if (InputNodeTask)
			{
				InputNodeTask->Inputs.Emplace(PreId);
			}

			// Hook nodes to the PreTask if they require so.
			// Only do it for nodes that are directly under the subgraph, not in subsequent subgraphs.
			for (FPCGGraphTask& Subtask : Subtasks)
			{
				if (!Subtask.Node || Subtask.Node->GetOuter() != Subgraph)
				{
					continue;
				}

				// We hook to pretask if the element requires data from the pretask, or otherwise if there are no inputs, to ensure
				// that the element executes with the other subgraph tasks (and can be culled if the subgraph node is culled).
				const UPCGSettings* Settings = Subtask.Node->GetSettings();
				const bool bRequiresDataFromPreTask = Settings && Settings->RequiresDataFromPreTask();
				if (bRequiresDataFromPreTask || Subtask.Inputs.IsEmpty())
				{
					Subtask.Inputs.Emplace(PreId, /*InUpstreamPin=*/FPCGGraphTaskInput::NoPin, /*InDownstreamPin=*/FPCGGraphTaskInput::NoPin, bRequiresDataFromPreTask);
				}
			}

			// Merge subgraph tasks into current tasks.
			CompiledTasks.Append(Subtasks);

			// Build post-task
			const FPCGTaskId PostId = NextId++;
			FPCGGraphTask& PostTask = CompiledTasks.Emplace_GetRef();
			PostTask.Node = Node;
			PostTask.NodeId = PostId;
			PostTask.StackIndex = InOutStackContext.GetCurrentStackIndex();
			// Implementation note: since we`ve already executed the node once, we normally don`t need to execute it a second time
			// especially since we cannot distinguish between the pre and post during execution so any data filtering related to pins is bound to fail.
			PostTask.Element = GetSharedTrivialElement();

			// Add execution-only dependency on pre-task, without this post task can be scheduled concurrently with pre-task, and concurrently
			// with something that might become inactive and would then fail to dynamically cull this already-scheduled task.
			// Additional implementation note: this first depedencency is critical in our ability to do static culling (see CalculateStaticallyActiveRecursive)
			// and should not be changed here without changing the other.
			PostTask.Inputs.Emplace(PreId, /*InUpstreamPin=*/FPCGGraphTaskInput::NoPin, /*InDownstreamPin=*/FPCGGraphTaskInput::NoPin, /*bInProvideData=*/false);

			// Add subgraph output node task as input to the post-task
			if (OutputNodeTask)
			{
				PostTask.Inputs.Emplace(OutputNodeTask->NodeId);
			}

			check(!IdMapping.Contains(Node));
			IdMapping.Add(Node, PostId);
		}
		else
		{
			const FPCGTaskId NodeId = NextId++;
			FPCGGraphTask& Task = CompiledTasks.Emplace_GetRef();
			Task.Node = Node;
			Task.NodeId = NodeId;
			Task.StackIndex = InOutStackContext.GetCurrentStackIndex();

			for (const UPCGPin* InputPin : Node->InputPins)
			{
				for (const UPCGEdge* InboundEdge : InputPin->Edges)
				{
					if (!InboundEdge->IsValid())
					{
						UE_LOG(LogPCG, Warning, TEXT("Unbound inbound edge"));
						continue;
					}

					if (FPCGTaskId* InboundId = IdMapping.Find(InboundEdge->InputPin->Node))
					{
						Task.Inputs.Emplace(*InboundId, InboundEdge->InputPin->Properties, InboundEdge->OutputPin->Properties);
					}
					else
					{
						UE_LOG(LogPCG, Error, TEXT("Inconsistent node linkage on node '%s'"), *Node->GetFName().ToString());
						return TArray<FPCGGraphTask>();
					}
				}
			}

			check(!IdMapping.Contains(Node));
			IdMapping.Add(Node, NodeId);
		}

		// Push next ready nodes on the queue
		for (const UPCGPin* OutPin : Node->OutputPins)
		{
			for (const UPCGEdge* OutboundEdge : OutPin->Edges)
			{
				if (!OutboundEdge->IsValid())
				{
					UE_LOG(LogPCG, Warning, TEXT("Unbound outbound edge"));
					continue;
				}

				const UPCGNode* OutboundNode = OutboundEdge->OutputPin->Node;
				check(OutboundNode);

				if (NodeQueue.Contains(OutboundNode))
				{
					continue;
				}

				bool bAllPrerequisitesMet = true;

				for (const UPCGPin* OutboundNodeInputPin : OutboundNode->InputPins)
				{
					for (const UPCGEdge* OutboundNodeInboundEdge : OutboundNodeInputPin->Edges)
					{
						if (OutboundNodeInboundEdge->IsValid())
						{
							bAllPrerequisitesMet &= IdMapping.Contains(OutboundNodeInboundEdge->InputPin->Node);
						}
					}
				}

				if (bAllPrerequisitesMet)
				{
					NodeQueue.Add(OutboundNode);
				}
			}
		}
	}

	return CompiledTasks;
}

void FPCGGraphCompiler::Compile(UPCGGraph* InGraph)
{
	GraphToTaskMapLock.ReadLock();
	bool bAlreadyCached = GraphToTaskMap.Contains(InGraph);
	GraphToTaskMapLock.ReadUnlock();

	if (bAlreadyCached)
	{
		return;
	}

	// Otherwise, do the compilation; note that we always start at zero since
	// the caller will offset the ids as needed
	FPCGTaskId FirstId = 0;
	FPCGStackContext StackContext;
	TArray<FPCGGraphTask> CompiledTasks = CompileGraph(InGraph, FirstId, StackContext);

	// TODO: optimize no-ops, etc.

	// Store back the results in the cache if it's valid
	if (!CompiledTasks.IsEmpty())
	{
		FWriteScopeLock Lock(GraphToTaskMapLock);
		if (!GraphToTaskMap.Contains(InGraph))
		{
			GraphToTaskMap.Add(InGraph, MoveTemp(CompiledTasks));
			GraphToStackContext.Add(InGraph, StackContext);
		}
	}
}

TArray<FPCGGraphTask> FPCGGraphCompiler::GetPrecompiledTasks(const UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph) const
{
	// Get compiled tasks in a threadsafe way
	FReadScopeLock ReadLock(GraphToTaskMapLock);

	const TArray<FPCGGraphTask>* ExistingTasks = nullptr;
	if (bIsTopGraph)
	{
		// Top graphs are optimized per grid size.
		const TMap<uint32, TArray<FPCGGraphTask>>* GridSizeToCompiledGraph = TopGraphToTaskMap.Find(InGraph);
		ExistingTasks = GridSizeToCompiledGraph ? GridSizeToCompiledGraph->Find(GenerationGridSize) : nullptr;

		const TMap<uint32, FPCGStackContext>* GridSizeToStackContext = TopGraphToStackContextMap.Find(InGraph);
		const FPCGStackContext* ExistingStackContext = GridSizeToStackContext ? GridSizeToStackContext->Find(GenerationGridSize) : nullptr;
		OutStackContext = ExistingStackContext ? *ExistingStackContext : FPCGStackContext();

		// Should either find both, or find neither.
		ensure(!ExistingTasks == !ExistingStackContext);
	}
	else
	{
		ExistingTasks = GraphToTaskMap.Find(InGraph);
	}

	return ExistingTasks ? *ExistingTasks : TArray<FPCGGraphTask>();
}

void FPCGGraphCompiler::ResolveGridSizes(
	EPCGHiGenGrid GenerationGrid,
	const TArray<FPCGGraphTask>& CompiledTasks,
	const FPCGStackContext& StackContext,
	EPCGHiGenGrid GenerationDefaultGrid,
	TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompiler::ResolveGridSizes);

	if (CompiledTasks.IsEmpty())
	{
		return;
	}
	check(!InOutTaskGenerationGrid.IsEmpty());

	// Special case - input node must always be present. Execute it on the current grid.
	InOutTaskGenerationGrid[0] = GenerationGrid;

	// Calculate execution grid values for subsequent tasks.
	for (int32 i = 1; i < CompiledTasks.Num(); ++i)
	{
		CalculateGridRecursive(CompiledTasks[i].NodeId, GenerationDefaultGrid, StackContext, CompiledTasks, InOutTaskGenerationGrid);
	}
}

void FPCGGraphCompiler::CreateGridLinkages(
	EPCGHiGenGrid InGenerationGrid,
	TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	const FPCGStackContext& InStackContext)
{
	// Now add link tasks - if a Grid256 task depends on data from a Grid512 task, inject a link
	// task that looks up the Grid512 component, schedules its execution if it does not have data, and
	// then uses its output data.
	// 
	// The stack is used to form the ResourceKey - a string that provides a path to the data from top graph down to specific pin.
	// This will be used by link tasks as store/retrieve keys to marshal data for edges that cross grid size boundaries.
	const FPCGStack* CurrentStack = InStackContext.GetStack(InStackContext.GetCurrentStackIndex());
	if (InOutCompiledTasks.IsEmpty() || !ensure(CurrentStack))
	{
		return;
	}

	const int32 NumCompiledTasksBefore = InOutCompiledTasks.Num();
	for (FPCGTaskId TaskId = 0; TaskId < NumCompiledTasksBefore; ++TaskId)
	{
		const EPCGHiGenGrid GraphGenerationGrid = InOutTaskGenerationGrid[TaskId];
		for (FPCGGraphTaskInput& TaskInput : InOutCompiledTasks[TaskId].Inputs)
		{
			const UPCGPin* UpstreamPin = nullptr;
			if (TaskInput.UpstreamPin.IsSet())
			{
				if (const UPCGNode* Node = InOutCompiledTasks[TaskInput.TaskId].Node)
				{
					UpstreamPin = Node->GetOutputPin(TaskInput.UpstreamPin.GetValue().Label);
				}
			}

			if (!UpstreamPin)
			{
				// Don't link if we don't have a upstream pin to retrieve data from
				continue;
			}

			const EPCGHiGenGrid InputGraphGenerationGrid = InOutTaskGenerationGrid[TaskInput.TaskId];
			// Register linkage task if grid sizes don't match - either way! This allows us to generate an execution-time error if going from
			// small grid to large grid.
			if (InputGraphGenerationGrid != EPCGHiGenGrid::Uninitialized && InputGraphGenerationGrid > GraphGenerationGrid)
			{
				// Build a string identifier for the data
				FString ResourceKey;
				if (!ensure(CurrentStack->CreateStackFramePath(ResourceKey, UpstreamPin->Node, UpstreamPin)))
				{
					continue;
				}

				// Build task & element to hold the operation to perform
				FPCGGraphTask& LinkTask = InOutCompiledTasks.Emplace_GetRef();
				LinkTask.NodeId = InOutCompiledTasks.Num() - 1;
				LinkTask.StackIndex = InOutCompiledTasks[TaskId].StackIndex;

				LinkTask.Inputs.Emplace(TaskInput.TaskId, UpstreamPin->Properties);

				const EPCGHiGenGrid FromGrid = InOutTaskGenerationGrid[TaskInput.TaskId];
				const EPCGHiGenGrid ToGrid = InOutTaskGenerationGrid[TaskId];

				// This lambda runs at execution time and attempts to retrieve the data from a larger grid. Capture by value is intentional.
				auto GridLinkageOperation = [FromGrid, ToGrid, ResourceKey, OutputPinLabel = TaskInput.UpstreamPin.GetValue().Label,
					DownstreamNode = InOutCompiledTasks[TaskId].Node, InGenerationGrid](FPCGContext* InContext)
				{
					return PCGGraphExecutor::ExecuteGridLinkage(
						InGenerationGrid,
						FromGrid,
						ToGrid,
						ResourceKey,
						OutputPinLabel,
						DownstreamNode,
						static_cast<FPCGGridLinkageContext*>(InContext));
				};

				FPCGGenericElement::FContextAllocator ContextAllocator = [](const FPCGDataCollection&, TWeakObjectPtr<UPCGComponent>, const UPCGNode*)
				{
					return new FPCGGridLinkageContext();
				};

				LinkTask.Element = MakeShared<PCGGraphExecutor::FPCGGridLinkageElement>(GridLinkageOperation, ContextAllocator, FromGrid, ToGrid, ResourceKey);

				// Now splice in the new task - redirect the downstream task to grab its input from the link task.
				TaskInput.TaskId = LinkTask.NodeId;

				// The link needs to execute at both FROM grid size (store) and TO grid size (retrieve).
				InOutTaskGenerationGrid.Add(FromGrid | ToGrid);
			}
		}
	}
}

EPCGHiGenGrid FPCGGraphCompiler::CalculateGridRecursive(
	FPCGTaskId InTaskId,
	EPCGHiGenGrid GenerationDefaultGrid,
	const FPCGStackContext& InStackContext,
	const TArray<FPCGGraphTask>& InCompiledTasks,
	TArray<EPCGHiGenGrid>& InOutTaskGenerationGrid)
{
	if (InOutTaskGenerationGrid[InTaskId] != EPCGHiGenGrid::Uninitialized)
	{
		return InOutTaskGenerationGrid[InTaskId];
	}

	const FPCGStack* Stack = InStackContext.GetStack(InCompiledTasks[InTaskId].StackIndex);
	check(Stack);
	const bool bTopLevelGraph = InCompiledTasks[InTaskId].ParentId == InvalidPCGTaskId;

	const UPCGNode* Node = InCompiledTasks[InTaskId].Node;
	const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
	const UPCGHiGenGridSizeSettings* GridSizeSettings = Cast<UPCGHiGenGridSizeSettings>(Settings);

	EPCGHiGenGrid Grid = EPCGHiGenGrid::Uninitialized;

	// Grid Size nodes in the top graph set the execution grid level.
	if (GridSizeSettings && GridSizeSettings->bEnabled && bTopLevelGraph)
	{
		Grid = FMath::Min(GenerationDefaultGrid, GridSizeSettings->GetGrid());
	}
	else
	{
		if (InCompiledTasks[InTaskId].Inputs.IsEmpty())
		{
			if (bTopLevelGraph)
			{
				// Tasks with no inputs in top graph get prescribed the default generation grid.
				Grid = GenerationDefaultGrid;
			}
			else
			{
				// Tasks with no inputs in a subgraph should execute on the same grid as the subgraph node task.
				check(InCompiledTasks[InTaskId].ParentId != InvalidPCGTaskId);
				Grid = CalculateGridRecursive(InCompiledTasks[InTaskId].ParentId, GenerationDefaultGrid, InStackContext, InCompiledTasks, InOutTaskGenerationGrid);
			}
		}
		else
		{
			// This task has inputs. Grid of this task is minimum of all input grids. We can link in data from a larger grid, but
			// not from a finer grid (this goes against hierarchy).
			Grid = EPCGHiGenGrid::Unbounded;
			for (FPCGGraphTaskInput InputTask : InCompiledTasks[InTaskId].Inputs)
			{
				const EPCGHiGenGrid InputGrid = CalculateGridRecursive(InputTask.TaskId, GenerationDefaultGrid, InStackContext, InCompiledTasks, InOutTaskGenerationGrid);
				if (PCGHiGenGrid::IsValidGrid(InputGrid))
				{
					Grid = FMath::Min(InputGrid, Grid);
				}
			}
		}
	}

	ensure(Grid != EPCGHiGenGrid::Uninitialized);

	InOutTaskGenerationGrid[InTaskId] = Grid;

	return Grid;
}

bool FPCGGraphCompiler::CalculateStaticallyActiveRecursive(FPCGTaskId InTaskId, const TArray<FPCGGraphTask>& InCompiledTasks, TMap<FPCGTaskId, bool>& InOutTaskIdToActiveFlag)
{
	if (const bool* bEntry = InOutTaskIdToActiveFlag.Find(InTaskId))
	{
		return *bEntry;
	}

	const UPCGNode* Node = InCompiledTasks[InTaskId].Node;

	// Nodes within subgraphs - if the subgraph node is inactive then all tasks within the subgraph are inactive.
	if (InCompiledTasks[InTaskId].ParentId != InvalidPCGTaskId)
	{
		const bool bParentActive = CalculateStaticallyActiveRecursive(InCompiledTasks[InTaskId].ParentId, InCompiledTasks, InOutTaskIdToActiveFlag);
		// With respect to the input node in a static subgraph, it has to use the same value as its parent since there is no direct edge between the subgraph node and this input node.
		if (!bParentActive || (Node && Node->GetSettings() && Node->GetSettings()->IsA<UPCGGraphInputOutputSettings>()))
		{
			InOutTaskIdToActiveFlag.Add(InTaskId, bParentActive);
			return bParentActive;
		}
	}

	if (!Node)
	{
		InOutTaskIdToActiveFlag.Add(InTaskId, true);
		return true;
	}

	// For static subgraphs, the second time this node is seen, it has a trivial task and no proper edge, which will trip the static validation below
	// This should basically forward the same information as the original node - which is conveniently the first input on that task.
	if (UPCGSubgraphSettings* SubgraphSettings = Cast<UPCGSubgraphSettings>(Node->GetSettings()))
	{
		FPCGTaskId TentativeSubgraphInputId = InCompiledTasks[InTaskId].Inputs.IsEmpty() ? InvalidPCGTaskId : InCompiledTasks[InTaskId].Inputs[0].TaskId;

		if (!SubgraphSettings->IsDynamicGraph() && TentativeSubgraphInputId != InvalidPCGTaskId && InCompiledTasks[TentativeSubgraphInputId].Node == Node)
		{
			const bool bSubgraphInputActive = CalculateStaticallyActiveRecursive(TentativeSubgraphInputId, InCompiledTasks, InOutTaskIdToActiveFlag);

			InOutTaskIdToActiveFlag.Add(InTaskId, bSubgraphInputActive);
			return bSubgraphInputActive;
		}
	}

	TArray<FName, TInlineAllocator<8>> PinsRequiringActiveConnection;

	// Three relevant types of input pins - required, non-advanced and advanced. This tracks if we encounter the second category.
	bool bHasAnyNonAdvancedPins = false;

	for (UPCGPin* InputPin : Node->GetInputPins())
	{
		if (!InputPin)
		{
			continue;
		}

		if (!InputPin->Properties.IsAdvancedPin())
		{
			bHasAnyNonAdvancedPins = true;

			if (Node->IsInputPinRequiredByExecution(InputPin))
			{
				PinsRequiringActiveConnection.AddUnique(InputPin->Properties.Label);
			}
		}
	}
	
	bool bHasAnyNonAdvancedInputs = false;
	bool bHasAnyActiveNonAdvancedInput = false;

	for (const FPCGGraphTaskInput& Input : InCompiledTasks[InTaskId].Inputs)
	{
		// Only non-advanced input pins play a part in determining active/inactive state.
		if (Input.DownstreamPin.IsSet() && Input.DownstreamPin.GetValue().IsAdvancedPin())
		{
			continue;
		}

		bHasAnyNonAdvancedInputs = true;

		// Default to tasks being active unless proved otherwise.
		bool bInputActive = true;

		// If we are connected to an upstream node, evaluate if the output pin is active.
		if (Input.UpstreamPin.IsSet())
		{
			const UPCGNode* UpstreamNode = InCompiledTasks[Input.TaskId].Node;
			if (const UPCGSettings* UpstreamSettings = UpstreamNode ? UpstreamNode->GetSettings() : nullptr)
			{
				bInputActive &= UpstreamSettings->IsPinStaticallyActive(Input.UpstreamPin.GetValue().Label);
			}
		}

		if (bInputActive)
		{
			bInputActive &= CalculateStaticallyActiveRecursive(Input.TaskId, InCompiledTasks, InOutTaskIdToActiveFlag);
		}

		if (bInputActive)
		{
			bHasAnyActiveNonAdvancedInput = true;

			if (Input.DownstreamPin.IsSet())
			{
				// Register received input on this pin.
				PinsRequiringActiveConnection.Remove(Input.DownstreamPin.GetValue().Label);
			}
		}
	}

	const UPCGSettings* Settings = Node->GetSettings();
	const bool bCanCullIfUnwired = Settings && Settings->CanCullTaskIfUnwired();

	bool bActive = true;

	if (!PinsRequiringActiveConnection.IsEmpty())
	{
		// If PinsRequiringActiveConnection is not empty then we did not find an input for each required pin.
		bActive = false;
	}
	else if (bCanCullIfUnwired)
	{
		// Cull if we have non-advanced pins but we don't have any active non-advanced inputs.
		bActive = !(bHasAnyNonAdvancedPins && !bHasAnyActiveNonAdvancedInput);
	}
	else if (bHasAnyNonAdvancedInputs)
	{
		// This task is allowed to be unwired, so we have non-advanced inputs and they're all inactive - basically
		// all upstream inputs are inactive which forces this task to be inactive.
		bActive = bHasAnyActiveNonAdvancedInput || InCompiledTasks[InTaskId].Inputs.IsEmpty();
	}

	InOutTaskIdToActiveFlag.Add(InTaskId, bActive);

	return bActive;
}

void FPCGGraphCompiler::CullTasksStaticInactive(TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	if (InOutCompiledTasks.IsEmpty())
	{
		return;
	}

	TMap<FPCGTaskId, bool> NodeIdToActiveFlag;
	// First task is input node task which is active
	NodeIdToActiveFlag.Add(InOutCompiledTasks[0].NodeId, true);

	for (int i = 1; i < InOutCompiledTasks.Num(); ++i)
	{
		// Results of each call memoized via NodeIdToActiveFlag.
		CalculateStaticallyActiveRecursive(InOutCompiledTasks[i].NodeId, InOutCompiledTasks, NodeIdToActiveFlag);
	}

	auto ShouldCull = [&NodeIdToActiveFlag](const FPCGGraphTask& InTask)
	{
		const bool* bActive = NodeIdToActiveFlag.Find(InTask.NodeId);
		return bActive && !*bActive;
	};
	CullTasks(InOutCompiledTasks, /*bAddPassthroughWires=*/false, ShouldCull);
}

void FPCGGraphCompiler::CullTasks(TArray<FPCGGraphTask>& InOutCompiledTasks, bool bAddPassthroughWires, TFunctionRef<bool(const FPCGGraphTask&)> CullTask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGraphCompiler::CullTasks);

	// Check that we have more than just the input task to work with.
	if (InOutCompiledTasks.Num() < 2)
	{
		return;
	}

	TArray<FPCGTaskId> TaskRemapping;
	TaskRemapping.SetNumUninitialized(InOutCompiledTasks.Num());

	// Mark culled tasks by remapping to INDEX_NONE. First task is input task and is never culled.
	TaskRemapping[0] = 0;
	for (int TaskIndex = 1; TaskIndex < InOutCompiledTasks.Num(); ++TaskIndex)
	{
		TaskRemapping[TaskIndex] = CullTask(InOutCompiledTasks[TaskIndex]) ? InvalidPCGTaskId : 0;
	}

	// Optionally add wires that bypass culled nodes.
	if (bAddPassthroughWires)
	{
		for (int TaskIndex = 1; TaskIndex < InOutCompiledTasks.Num(); ++TaskIndex)
		{
			FPCGGraphTask& Task = InOutCompiledTasks[TaskIndex];
			if (!Task.Node || Task.Node->GetInputPins().IsEmpty())
			{
				continue;
			}

			int InputIndex = 0;
			while (InputIndex < Task.Inputs.Num())
			{
				const FPCGTaskId InputTaskId = Task.Inputs[InputIndex].TaskId;

				// Is node culled?
				if (TaskRemapping[InputTaskId] == InvalidPCGTaskId)
				{
					const FPCGGraphTask& CulledInputTask = InOutCompiledTasks[InputTaskId];
					if (CulledInputTask.Node && CulledInputTask.Node->GetInputPins().Num() > 1)
					{
						ensureMsgf(false, TEXT("Task culling currently only supports nodes with a single input pin which are trivial to unwire."));
						continue;
					}

					// Upstream node was culled. To preserve order, insert new wires just after the wire to be culled--which will be removed later.
					Task.Inputs.Insert(CulledInputTask.Inputs, InputIndex + 1);

					const int NewWireCount = CulledInputTask.Inputs.Num();
					for (int I = 0; I < NewWireCount; ++I)
					{
						Task.Inputs[InputIndex + 1 + I].DownstreamPin = Task.Inputs[InputIndex].DownstreamPin;
					}

					// Skip evaluating the newly wired inputs and increment
					InputIndex += NewWireCount;
				}

				++InputIndex;
			}
		}
	}

	// Remove all culled tasks by compacting the task array. Never cull first (Input) task.
	int WriteIndex = 1;
	int ReadIndex = 1;
	while (ReadIndex < InOutCompiledTasks.Num())
	{
		// If not culled, then move the task to it's final remapped position in the task array.
		if (TaskRemapping[ReadIndex] != InvalidPCGTaskId)
		{
			if (WriteIndex != ReadIndex)
			{
				InOutCompiledTasks[WriteIndex] = MoveTemp(InOutCompiledTasks[ReadIndex]);
				InOutCompiledTasks[WriteIndex].NodeId = WriteIndex;
			}

			TaskRemapping[ReadIndex] = WriteIndex;
			++WriteIndex;
		}

		++ReadIndex;
	}

	InOutCompiledTasks.SetNum(WriteIndex);

	for (FPCGGraphTask& Task : InOutCompiledTasks)
	{
		// Remap input task IDs, and remove edges that connect to culled nodes.
		for (int InputIndex = Task.Inputs.Num() - 1; InputIndex >= 0; --InputIndex)
		{
			const FPCGTaskId InputTaskId = Task.Inputs[InputIndex].TaskId;
			const FPCGTaskId Remap = TaskRemapping[InputTaskId];
			if (Remap != InvalidPCGTaskId)
			{
				Task.Inputs[InputIndex].TaskId = Remap;
			}
			else
			{
				Task.Inputs.RemoveAt(InputIndex);
			}
		}

		// Remap parent ID if there tasks is in a child scope.
		if (Task.ParentId != InvalidPCGTaskId)
		{
			const FPCGTaskId RemappedParentId = TaskRemapping[Task.ParentId];
			// Parent task should not have been culled.
			ensure(RemappedParentId != InvalidPCGTaskId);

			// Write the remapped ID - even if it's invalid/INDEX_NONE. Hanging parent IDs can cause issues elsewhere.
			Task.ParentId = RemappedParentId;
		}
	}
}

void FPCGGraphCompiler::PostCullStackCleanup(TArray<FPCGGraphTask>& InCompiledTasks, FPCGStackContext& InOutStackContext)
{
	// Build set of stack IDs used by tasks. Using array as set is likely small.
	TArray<int32> ActiveStackIDs;
	for (FPCGGraphTask& Task : InCompiledTasks)
	{
		ActiveStackIDs.AddUnique(Task.StackIndex);
	}

	if (ActiveStackIDs.Num() < InOutStackContext.GetNumStacks())
	{
		// Prepare cumulative counts for stack ID remapping.
		TArray<int> RemovedCounts;
		RemovedCounts.SetNumZeroed(InOutStackContext.GetNumStacks());

		// Remove any stacks that are inactive.
		TArray<FPCGStack>& Stacks = InOutStackContext.GetStacksMutable();
		for (int StackIndex = Stacks.Num() - 1; StackIndex >= 0; --StackIndex)
		{
			if (!ActiveStackIDs.Contains(StackIndex))
			{
				Stacks.RemoveAt(StackIndex);
				RemovedCounts[StackIndex] = 1;
			}
		}

		// Accumulate removed counts.
		for (int Index = 1; Index < RemovedCounts.Num(); ++Index)
		{
			RemovedCounts[Index] += RemovedCounts[Index - 1];
		}

		// Remap stack IDs.
		for (FPCGGraphTask& Task : InCompiledTasks)
		{
			Task.StackIndex -= RemovedCounts[Task.StackIndex];
		}
	}
}

void FPCGGraphCompiler::CalculateDynamicActivePinDependencies(FPCGTaskId InTaskId, TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	if (!ensure(InOutCompiledTasks.IsValidIndex(InTaskId)))
	{
		return;
	}

	const UPCGNode* Node = InOutCompiledTasks[InTaskId].Node;

	// First if there are any required pins, build the pin dependencies from these. We have an array of inputs, each one can
	// correspond to a pin. Use a map to compile an expression for each input pin label.
	TMap<FName, FPCGPinDependencyExpression> InputPinLabelToPinDependency;

	for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[InTaskId].Inputs)
	{
		if (!Node || !Input.DownstreamPin.IsSet())
		{
			continue;
		}

		const UPCGPin* InputPin = Node->GetInputPin(Input.DownstreamPin.GetValue().Label);

		// Consider only primary input pins in this pass.
		if (!InputPin || !Node->IsInputPinRequiredByExecution(InputPin))
		{
			continue;
		}

		const UPCGNode* UpstreamNode = InOutCompiledTasks[Input.TaskId].Node;
		if (!UpstreamNode)
		{
			continue;
		}

		const int PinIndex = UpstreamNode->GetOutputPins().IndexOfByPredicate([&Input](const UPCGPin* InPin)
		{
			return InPin->Properties == Input.UpstreamPin;
		});

		if (PinIndex != INDEX_NONE)
		{
			check(PinIndex < PCGPinIdHelpers::MaxOutputPins);

			FPCGPinDependencyExpression& PinDependency = InputPinLabelToPinDependency.FindOrAdd(Input.DownstreamPin.GetValue().Label);
			PinDependency.AddPinDependency(PCGPinIdHelpers::NodeIdAndPinIndexToPinId(Input.TaskId, PinIndex));
		}
	}

	// Result expression. Conjunction of disjunctions of pin IDs that are required to be active for this task to be active.
	// Example - keep task if: UpstreamPin0Active && (UpstreamPin1Active || UpstreamPin2Active)
	FPCGPinDependencyExpression PinDependency;

	if (!InputPinLabelToPinDependency.IsEmpty())
	{
		// If we have registered pin dependencies from the first pass (required pins) then we have can compose these for our pin
		// dependency expression. Build conjunction from the per-pin disjunctions.
		for (TPair<FName, FPCGPinDependencyExpression>& PinExpression : InputPinLabelToPinDependency)
		{
			PinDependency.AppendUsingConjunction(PinExpression.Value);
		}
	}
	else
	{
		// In the case of output nodes, advanced pins shouldn't be ignored as it can and will prevent culling in some instances
		const bool bTreatAdvancedPinsAsNormal = (Node && Cast<UPCGGraphInputOutputSettings>(Node->GetSettings()) && !Cast<UPCGGraphInputOutputSettings>(Node->GetSettings())->IsInput());

		// If we don't have any dependent pins logged by now then there are no required pins (note that a node that does not have task
		// inputs for required pins will be statically culled in an earlier compilation step). In which case we'll be active if *any* input
		// is active. We build a disjunction that expresses this.
		for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[InTaskId].Inputs)
		{
			if (Input.DownstreamPin.IsSet() && Input.DownstreamPin.GetValue().IsAdvancedPin() && !bTreatAdvancedPinsAsNormal)
			{
				// Advanced input pins never participate in keeping node active.
				continue;
			}
		
			if (Input.UpstreamPin.IsSet())
			{
				// Input connection is via node pins.
				const UPCGNode* UpstreamNode = InOutCompiledTasks[Input.TaskId].Node;
				if (!UpstreamNode)
				{
					continue;
				}

				const int PinIndex = UpstreamNode->GetOutputPins().IndexOfByPredicate([&Input](const UPCGPin* InPin)
				{
					return InPin->Properties == Input.UpstreamPin.GetValue();
				});

				if (PinIndex != INDEX_NONE)
				{
					check(PinIndex < PCGPinIdHelpers::MaxOutputPins);

					PinDependency.AddPinDependency(PCGPinIdHelpers::NodeIdAndPinIndexToPinId(Input.TaskId, PinIndex));
				}
			}
			else
			{
				// No associated pin, use a special pin ID for a pin-less dependency.
				PinDependency.AddPinDependency(PCGPinIdHelpers::NodeIdToPinId(Input.TaskId));
			}
		}
	}

	InOutCompiledTasks[InTaskId].PinDependency = MoveTemp(PinDependency);
}

TArray<FPCGGraphTask> FPCGGraphCompiler::GetCompiledTasks(UPCGGraph* InGraph, uint32 GenerationGridSize, FPCGStackContext& OutStackContext, bool bIsTopGraph)
{
	TArray<FPCGGraphTask> CompiledTasks;

	if (bIsTopGraph)
	{
		// Always try to compile
		CompileTopGraph(InGraph, GenerationGridSize);

		// Get compiled tasks in a threadsafe way
		FReadScopeLock Lock(GraphToTaskMapLock);
		const TMap<uint32, TArray<FPCGGraphTask>>* GridSizeToCompiledGraph = TopGraphToTaskMap.Find(InGraph);
		const TArray<FPCGGraphTask>* Tasks = GridSizeToCompiledGraph ? GridSizeToCompiledGraph->Find(GenerationGridSize) : nullptr;

		const TMap<uint32, FPCGStackContext>* GridSizeToStackContext = TopGraphToStackContextMap.Find(InGraph);
		const FPCGStackContext* StackContext = GridSizeToStackContext ? GridSizeToStackContext->Find(GenerationGridSize) : nullptr;

		// Should have either found both or neither.
		ensure(!Tasks == !StackContext);

		if (Tasks && StackContext)
		{
			CompiledTasks = *Tasks;
			OutStackContext = *StackContext;
		}
	}
	else
	{
		// Always try to compile
		Compile(InGraph);

		// Get compiled tasks in a threadsafe way
		FReadScopeLock Lock(GraphToTaskMapLock);
		if (TArray<FPCGGraphTask>* Tasks = GraphToTaskMap.Find(InGraph))
		{
			CompiledTasks = *Tasks;
			OutStackContext = GraphToStackContext[InGraph];
		}
	}

	return CompiledTasks;
}

void FPCGGraphCompiler::OffsetNodeIds(TArray<FPCGGraphTask>& Tasks, FPCGTaskId Offset, FPCGTaskId ParentId)
{
	for (FPCGGraphTask& Task : Tasks)
	{
		Task.NodeId += Offset;

		if (Task.ParentId == InvalidPCGTaskId)
		{
			Task.ParentId = ParentId;
		}
		else
		{
			Task.ParentId += Offset;
		}

		for (FPCGGraphTaskInput& Input : Task.Inputs)
		{
			Input.TaskId += Offset;
		}

		Task.PinDependency.OffsetNodeIds(Offset);
	}
}

void FPCGGraphCompiler::CompileTopGraph(UPCGGraph* InGraph, uint32 GenerationGridSize)
{
	GraphToTaskMapLock.ReadLock();
	const TMap<uint32, TArray<FPCGGraphTask>>* GridSizeToCompiledGraph = TopGraphToTaskMap.Find(InGraph);
	const bool bAlreadyCached = GridSizeToCompiledGraph && GridSizeToCompiledGraph->Contains(GenerationGridSize);
	GraphToTaskMapLock.ReadUnlock();

	if (bAlreadyCached)
	{
		return;
	}

	UE_LOG(LogPCG, Verbose, TEXT("FPCGGraphCompiler::CompileTopGraph '%s' grid: %u"), *InGraph->GetName(), GenerationGridSize);

	// Build from non-top tasks
	FPCGStackContext StackContext;
	TArray<FPCGGraphTask> CompiledTasks = GetCompiledTasks(InGraph, GenerationGridSize, StackContext, /*bIsTopGraph=*/false);

	// Check that the compilation was valid
	if (CompiledTasks.Num() == 0)
	{
		return;
	}

	if (PCGGraphCompiler::CVarEnableTaskStaticCulling.GetValueOnAnyThread())
	{
		// Remove reroute nodes before execution grid setup, as grid linkages need final nodes to connect from/to.
		CullTasks(CompiledTasks, /*bAddPassthroughWires=*/true, [](const FPCGGraphTask& InTask) { return InTask.Node && Cast<UPCGRerouteSettings>(InTask.Node->GetSettings()) && InTask.Node->HasInboundEdges(); });

		// Cull inactive branches downstream of branch nodes with static selection values.
		CullTasksStaticInactive(CompiledTasks);

		// For hierarchical generation resolve the execution grid for each task and cull any tasks that won't execute.
		// TODO - we could add an else branch for higen disabled that culls the grid size nodes.
		if (InGraph->IsHierarchicalGenerationEnabled() && GenerationGridSize != PCGHiGenGrid::UninitializedGridSize())
		{
			const EPCGHiGenGrid GenerationGrid = PCGHiGenGrid::GridSizeToGrid(GenerationGridSize);
			const EPCGHiGenGrid DefaultGrid = PCGHiGenGrid::GridSizeToGrid(InGraph->GetDefaultGridSize());

			// Propagate grid size nodes through the graph to determine which grid size each task should execute on.
			TArray<EPCGHiGenGrid> TaskGenerationGrid;
			TaskGenerationGrid.SetNumZeroed(CompiledTasks.Num());
			ResolveGridSizes(GenerationGrid, CompiledTasks, StackContext, DefaultGrid, TaskGenerationGrid);

			// Create linkage tasks for edges that cross from large grid to small grid tasks.
			CreateGridLinkages(GenerationGrid, TaskGenerationGrid, CompiledTasks, StackContext);

			// Cull any task that should not execute on the current grid.
			CullTasks(CompiledTasks, /*bAddPassthroughWires=*/false, [GenerationGrid, &TaskGenerationGrid](const FPCGGraphTask& InTask)
			{
				const EPCGHiGenGrid TaskGrid = TaskGenerationGrid[InTask.NodeId];
				return TaskGrid != EPCGHiGenGrid::Uninitialized && !(TaskGrid & GenerationGrid);
			});
		}
	}

	// TODO: GPU execution is editor only for now. Need to cook kernels for standalone.
#if WITH_EDITOR
	if (PCGGraphCompiler::CVarEnableGPUExecution.GetValueOnAnyThread())
	{
		CreateGPUNodes(InGraph, CompiledTasks);
	}
	else
#endif
	{
		// GPU not supported. Cull all the GPU compatible nodes without adding passthrough wires. This is destructive!
		// TODO: In the future when we have nodes that can target both CPU and GPU, this could just force CPU (and throw
		// errors if nodes strictly require GPU).
		int CountBefore = CompiledTasks.Num();

		CullTasks(CompiledTasks, /*bAddPassthroughWires=*/false, [](const FPCGGraphTask& InTask)
		{
			const UPCGSettings* Settings = InTask.Node ? InTask.Node->GetSettings() : nullptr;
			return Settings && Settings->ShouldExecuteOnGPU();
		});

		if (CountBefore > CompiledTasks.Num())
		{
			UE_LOG(LogPCG, Warning, TEXT("One or more GPU nodes were culled from the graph!"));
		}
	}

	// Post culling - remove any stacks that are no longer part of execution. Besides being tidy this also helps
	// debug tools discern which stacks were executed or not.
	PostCullStackCleanup(CompiledTasks, StackContext);

	// To feed dynamic culling at execution time, build list of upstream pins that we depend on (if all of these pins
	// are determined to be inactive at execution time, then the task will be deactivated). An empty list means the
	// task will never be deactivated and will always execute.
	// TODO: Expand pin dependencies to pure branches that aren't directly downstream - nodes that have no side effects
	// and feed into a branch can also be culled if the branch is culled.
	// TODO: Pin dependencies should be transitive across nodes. If a node is dependent on a single upstream node, it could
	// likely take the pin dependencies from the upstream node, which should save iterations in the dynamic culling code.
	for (int TaskIndex = 0; TaskIndex < CompiledTasks.Num(); ++TaskIndex)
	{
		// Result is written directly to tasks.
		CalculateDynamicActivePinDependencies(CompiledTasks[TaskIndex].NodeId, CompiledTasks);
	}

	const int TaskNum = CompiledTasks.Num();
	const FPCGTaskId PreExecuteTaskId = FPCGTaskId(TaskNum);
	const FPCGTaskId PostExecuteTaskId = PreExecuteTaskId + 1;

	FPCGGraphTask& PreExecuteTask = CompiledTasks.Emplace_GetRef();
	PreExecuteTask.Element = GetSharedTrivialElement();
	PreExecuteTask.NodeId = PreExecuteTaskId;

	for (int TaskIndex = 0; TaskIndex < TaskNum; ++TaskIndex)
	{
		FPCGGraphTask& Task = CompiledTasks[TaskIndex];
		if (Task.Inputs.IsEmpty())
		{
			Task.Inputs.Emplace(PreExecuteTaskId);
		}

		Task.CompiledTaskId = Task.NodeId;
	}

	FPCGGraphTask& PostExecuteTask = CompiledTasks.Emplace_GetRef();
	PostExecuteTask.Element = GetSharedTrivialPostGraphElement();
	PostExecuteTask.NodeId = PostExecuteTaskId;

	// Find end nodes, e.g. all nodes that have no successors.
	// In our representation we don't have this, so find it out by going backwards
	// Note: this works because there is a weak ordering on the tasks such that
	// a successor task is always after its predecessors
	TSet<FPCGTaskId> TasksWithSuccessors;
	const UPCGNode* GraphOutputNode = InGraph->GetOutputNode();

	for (int TaskIndex = TaskNum - 1; TaskIndex >= 0; --TaskIndex)
	{
		const FPCGGraphTask& Task = CompiledTasks[TaskIndex];
		if (!TasksWithSuccessors.Contains(Task.NodeId))
		{
			// For the post task, only the output node will provide data. 
			// It is necessary for any post generation task to get the content of the output node
			// and only this content.
			const bool bProvideData = Task.Node == GraphOutputNode;
			PostExecuteTask.Inputs.Emplace(Task.NodeId, /*InUpstreamPin=*/FPCGGraphTaskInput::NoPin, /*InDownstreamPin=*/FPCGGraphTaskInput::NoPin, bProvideData);
		}

		for (const FPCGGraphTaskInput& Input : Task.Inputs)
		{
			TasksWithSuccessors.Add(Input.TaskId);
		}
	}

	// Store back the results in the cache
	GraphToTaskMapLock.WriteLock();
	TMap<uint32, TArray<FPCGGraphTask>>& TasksPerGenerationGrid = TopGraphToTaskMap.FindOrAdd(InGraph);
	if (!TasksPerGenerationGrid.Contains(GenerationGridSize))
	{
		TasksPerGenerationGrid.Add(GenerationGridSize, MoveTemp(CompiledTasks));
	}

	TMap<uint32, FPCGStackContext>& StackContextPerGenerationGrid = TopGraphToStackContextMap.FindOrAdd(InGraph);
	if (!StackContextPerGenerationGrid.Contains(GenerationGridSize))
	{
		StackContextPerGenerationGrid.Add(GenerationGridSize, MoveTemp(StackContext));
	}
	GraphToTaskMapLock.WriteUnlock();
}

FPCGElementPtr FPCGGraphCompiler::GetSharedTrivialElement()
{
	{
		FReadScopeLock Lock(SharedTrivialElementLock);

		if (SharedTrivialElement)
		{
			return SharedTrivialElement;
		}
	}

	FWriteScopeLock Lock(SharedTrivialElementLock);

	if (!SharedTrivialElement)
	{
		SharedTrivialElement = MakeShared<FPCGTrivialElement>();
	}

	return SharedTrivialElement;
}

FPCGElementPtr FPCGGraphCompiler::GetSharedGatherElement()
{
	{
		FReadScopeLock Lock(SharedGatherElementLock);

		if (SharedGatherElement)
		{
			return SharedGatherElement;
		}
	}

	FWriteScopeLock Lock(SharedGatherElementLock);

	if (!SharedGatherElement)
	{
		SharedGatherElement = MakeShared<FPCGGatherElement>();
	}

	return SharedGatherElement;
}

FPCGElementPtr FPCGGraphCompiler::GetSharedTrivialPostGraphElement()
{
	{
		FReadScopeLock Lock(SharedTrivialElementLock);

		if (SharedTrivialPostGraphElement)
		{
			return SharedTrivialPostGraphElement;
		}
	}

	FWriteScopeLock Lock(SharedTrivialElementLock);

	if (!SharedTrivialPostGraphElement)
	{
		SharedTrivialPostGraphElement = MakeShared<FPCGTrivialElement>();
	}

	return SharedTrivialPostGraphElement;
}

void FPCGGraphCompiler::ClearCache()
{
	FWriteScopeLock Lock(GraphToTaskMapLock);
	GraphToTaskMap.Reset();
	GraphToStackContext.Reset();
	TopGraphToTaskMap.Reset();
	TopGraphToStackContextMap.Reset();
}

#if WITH_EDITOR
void FPCGGraphCompiler::NotifyGraphChanged(UPCGGraph* InGraph, EPCGChangeType ChangeType)
{
	if (InGraph && (ChangeType != EPCGChangeType::Cosmetic))
	{
		RemoveFromCache(InGraph);
	}
}

bool FPCGGraphCompiler::Recompile(UPCGGraph* InGraph, uint32 GenerationGridSize, bool bIsTopGraph)
{
	FPCGStackContext StackContextBefore;
	const TArray<FPCGGraphTask> TasksBefore = GetPrecompiledTasks(InGraph, GenerationGridSize, StackContextBefore, bIsTopGraph);

	// Need to manually purge as the graph compiler will not have gotten the change notification yet. Editor only.
	RemoveFromCache(InGraph);

	FPCGStackContext StackContextAfter;
	const TArray<FPCGGraphTask> TasksAfter = GetCompiledTasks(InGraph, GenerationGridSize, StackContextAfter, bIsTopGraph);

	bool bAllTasksEqual = TasksBefore.Num() == TasksAfter.Num();
	if (bAllTasksEqual)
	{
		for (int I = 0; I < TasksBefore.Num(); ++I)
		{
			bAllTasksEqual = bAllTasksEqual && TasksAfter[I].IsApproximatelyEqual(TasksBefore[I]);
		}
	}

	// Compiled result is compiled tasks + associated stacks. Compare both and return true if compiled result changes.
	return !bAllTasksEqual || (StackContextBefore != StackContextAfter);
}

void FPCGGraphCompiler::RemoveFromCache(UPCGGraph* InGraph)
{
	UE_LOG(LogPCG, Verbose, TEXT("FPCGGraphCompiler::RemoveFromCache '%s'"), *InGraph->GetName());

	check(InGraph);
	RemoveFromCacheRecursive(InGraph);
}

void FPCGGraphCompiler::RemoveFromCacheRecursive(UPCGGraph* InGraph)
{
	GraphToTaskMapLock.WriteLock();
	GraphToTaskMap.Remove(InGraph);
	GraphToStackContext.Remove(InGraph);
	TopGraphToTaskMap.Remove(InGraph);
	TopGraphToStackContextMap.Remove(InGraph);
	GraphToTaskMapLock.WriteUnlock();

	GraphDependenciesLock.Lock();
	TArray<UPCGGraph*> ParentGraphs;
	GraphDependencies.MultiFind(InGraph, ParentGraphs);
	GraphDependencies.Remove(InGraph);
	GraphDependenciesLock.Unlock();

	for (UPCGGraph* Graph : ParentGraphs)
	{
		RemoveFromCacheRecursive(Graph);
	}
}
#endif // WITH_EDITOR

void FPCGGraphCompiler::CollectGPUNodeSubsets(const TArray<FPCGGraphTask>& InCompiledTasks, const TMap<FPCGTaskId, TArray<FPCGTaskId>>& InTaskSuccessors, const TSet<FPCGTaskId>& InGPUCompatibleTaskIds, TArray<TSet<FPCGTaskId>>& OutNodeSubsetsToConvertToCFGraph)
{
	// Populate initial sets of tasks that are ready to consume vs ones that currently blocked.
	TSet<FPCGTaskId> ReadyTaskIds;
	TSet<FPCGTaskId> RemainingTaskIds;
	ReadyTaskIds.Reserve(InCompiledTasks.Num());
	RemainingTaskIds.Reserve(InCompiledTasks.Num());

	for (FPCGTaskId TaskId = 0; TaskId < InCompiledTasks.Num(); ++TaskId)
	{
		if (InCompiledTasks[TaskId].Inputs.IsEmpty())
		{
			ReadyTaskIds.Add(TaskId);
		}
		else
		{
			RemainingTaskIds.Add(TaskId);
		}
	}

	// Queue all successors of InTaskId that are ready to go (all upstream input tasks have been processed).
	auto QueueSuccessors = [&InTaskSuccessors, &ReadyTaskIds, &RemainingTaskIds, &InCompiledTasks](int InTaskId)
	{
		bool bQueuedTask = false;

		// Queue up any successors that are ready to go.
		if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(InTaskId))
		{
			for (FPCGTaskId Successor : *Successors)
			{
				const bool bSuccessorQueued = ReadyTaskIds.Contains(Successor);

				// All successors should either be already queued, or waiting to be queued.
				check(bSuccessorQueued || RemainingTaskIds.Contains(Successor));

				if (!bSuccessorQueued)
				{
					bool bSuccessorReady = true;
					for (const FPCGGraphTaskInput& Input : InCompiledTasks[Successor].Inputs)
					{
						if (ReadyTaskIds.Contains(Input.TaskId) || RemainingTaskIds.Contains(Input.TaskId))
						{
							bSuccessorReady = false;
						}
					}

					if (bSuccessorReady)
					{
						ReadyTaskIds.Add(Successor);
						RemainingTaskIds.Remove(Successor);
						bQueuedTask = true;
					}
				}
			}
		}

		return bQueuedTask;
	};
	
	// Local to loops below but pulled out for performance.
	TArray<FPCGTaskId> FoundReadyTaskIds;
	TSet<FPCGTaskId> GPUSubsetTaskIds;
	FoundReadyTaskIds.Reserve(InCompiledTasks.Num());
	GPUSubsetTaskIds.Reserve(InCompiledTasks.Num());

	// Build subsets of nodes that are GPU compatible and can be dispatched together.
	while (!ReadyTaskIds.IsEmpty() || !RemainingTaskIds.IsEmpty())
	{
		// Consume as many CPU nodes as we can.
		bool bQueuedTasks = true;
		while (bQueuedTasks)
		{
			FoundReadyTaskIds.Reset();

			for (FPCGTaskId ReadyTaskId : ReadyTaskIds)
			{
				if (!InGPUCompatibleTaskIds.Contains(ReadyTaskId))
				{
					FoundReadyTaskIds.Add(ReadyTaskId);
				}
			}

			bQueuedTasks = false;

			for (FPCGTaskId ReadyCPUTaskId : FoundReadyTaskIds)
			{
				ReadyTaskIds.Remove(ReadyCPUTaskId);
				bQueuedTasks |= QueueSuccessors(ReadyCPUTaskId);
			}
		}

		GPUSubsetTaskIds.Reset();

		int StackIndex = INDEX_NONE;

		// Now the opposite - consume as many GPU nodes as we can and accumulate them into a set that will be compiled into a compute graph.
		bQueuedTasks = !ReadyTaskIds.IsEmpty();
		while (bQueuedTasks)
		{
			FoundReadyTaskIds.Reset();

			for (FPCGTaskId ReadyTaskId : ReadyTaskIds)
			{
				if (InGPUCompatibleTaskIds.Contains(ReadyTaskId))
				{
					// For now don't mix tasks from different execution stacks (in and out of subgraphs for instance) into one compute graph.
					if ((StackIndex == INDEX_NONE) || InCompiledTasks[ReadyTaskId].StackIndex == StackIndex)
					{
						StackIndex = InCompiledTasks[ReadyTaskId].StackIndex;
						FoundReadyTaskIds.Add(ReadyTaskId);
					}
				}
			}

			bQueuedTasks = false;

			for (FPCGTaskId ReadyGPUTaskId : FoundReadyTaskIds)
			{
				GPUSubsetTaskIds.Add(ReadyGPUTaskId);
				ReadyTaskIds.Remove(ReadyGPUTaskId);
				bQueuedTasks |= QueueSuccessors(ReadyGPUTaskId);
			}
		}

		if (!GPUSubsetTaskIds.IsEmpty())
		{
			OutNodeSubsetsToConvertToCFGraph.Add(MoveTemp(GPUSubsetTaskIds));
		}
	}
}

void FPCGGraphCompiler::CreateGatherTasksAtGPUInputs(const TSet<FPCGTaskId>& InGPUCompatibleTaskIds, TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	using FOriginalInputPinKey = TPair<FPCGTaskId /* Original GPU task */, FName /* Input pin label */>;

	// These are local to loop below but hoisted here for efficiency.
	TSet<FOriginalInputPinKey> EncounteredInputPins; // TODO this is heavyweight, could use simple array.
	TMap<FOriginalInputPinKey, FPCGTaskId /* Gather task */> InputPinToGatherTask;

	// Add all compute graph task inputs and outputs.
	for (FPCGTaskId GPUTaskId : InGPUCompatibleTaskIds)
	{
		EncounteredInputPins.Reset();
		InputPinToGatherTask.Reset();

		// First pass - create gather tasks for all original input pins which have more than one incident edge.
		// This is so we can gather on the CPU (much more efficient than going it on the GPU).
		for (int InputIndex = 0; InputIndex < InOutCompiledTasks[GPUTaskId].Inputs.Num(); ++InputIndex)
		{
			// Helper to get current input. We avoid simply taking a local reference as InOutCompiledTasks can be modified below.
			auto CurrentInput = [&InOutCompiledTasks, InputIndex, GPUTaskId]() -> FPCGGraphTaskInput&
			{
				return InOutCompiledTasks[GPUTaskId].Inputs[InputIndex];
			};

			if (!CurrentInput().DownstreamPin.IsSet())
			{
				continue;
			}

			const FOriginalInputPinKey PinKey = { GPUTaskId, CurrentInput().DownstreamPin->Label };

			// If already created a gather task, then nothing more to do for this pin.
			if (const FPCGTaskId* GatherTaskId = InputPinToGatherTask.Find(PinKey))
			{
				continue;
			}

			// If we're encountering pin for first time, register it.
			if (!EncounteredInputPins.Contains(PinKey))
			{
				EncounteredInputPins.Add(PinKey);
				continue;
			}

			// Second time we've encountered this input pin - create a gather element because we need one edge connected to
			// each virtual input pin, so that we can obtain the data items from the input data collection using the unique
			// virtual pin label at execution time.
			const FPCGTaskId GatherTaskId = InOutCompiledTasks.Num();
			FPCGGraphTask& GatherTask = InOutCompiledTasks.Emplace_GetRef();
			GatherTask.NodeId = GatherTaskId;
			GatherTask.ParentId = InOutCompiledTasks[GPUTaskId].ParentId;
			GatherTask.Element = GetSharedGatherElement();

			InputPinToGatherTask.Add(PinKey, GatherTaskId);
		}

		EncounteredInputPins.Reset();

		// Second pass - wire up the newly added gather tasks once we have the full picture of which edges are affected.
		for (int InputIndex = 0; InputIndex < InOutCompiledTasks[GPUTaskId].Inputs.Num(); ++InputIndex)
		{
			auto CurrentInput = [&InOutCompiledTasks, InputIndex, GPUTaskId]() -> FPCGGraphTaskInput&
			{
				return InOutCompiledTasks[GPUTaskId].Inputs[InputIndex];
			};

			if (!CurrentInput().DownstreamPin.IsSet())
			{
				continue;
			}

			const FOriginalInputPinKey PinKey = { GPUTaskId, CurrentInput().DownstreamPin->Label };

			if (const FPCGTaskId* GatherTaskId = InputPinToGatherTask.Find(PinKey))
			{
				// Wire the upstream output pin to the gather task.
				FPCGGraphTaskInput& WireUpstreamNodeToGather = InOutCompiledTasks[*GatherTaskId].Inputs.Add_GetRef(CurrentInput());
				if (WireUpstreamNodeToGather.DownstreamPin.IsSet())
				{
					WireUpstreamNodeToGather.DownstreamPin->Label = PCGPinConstants::DefaultInputLabel;
				}

				if (!EncounteredInputPins.Contains(PinKey))
				{
					// First time we're encountering this input pin, wire it to the gather task.
					EncounteredInputPins.Add(PinKey);

					CurrentInput().TaskId = *GatherTaskId;
					if (CurrentInput().UpstreamPin.IsSet())
					{
						CurrentInput().UpstreamPin->Label = PCGPinConstants::DefaultOutputLabel;
					}
				}
				else
				{
					// Input pin already encountered, already wired to gather task. Remove this input.
					InOutCompiledTasks[GPUTaskId].Inputs.RemoveAt(InputIndex);
					--InputIndex;
				}
			}
		}
	}
}

void FPCGGraphCompiler::WireGPUGraphNode(
	FPCGTaskId InGPUGraphTaskId,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TSet<FPCGTaskId>& InGPUCompatibleTaskIds,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	const TMap<FPCGTaskId, TArray<FPCGTaskId>>& InTaskSuccessors,
	FOriginalToVirtualPin& OutOriginalToVirtualPin,
	TMap<const UPCGPin*, FName>& OutOutputCPUPinToVirtualPin)
{
	FPCGGraphTask& GPUGraphTask = InOutCompiledTasks[InGPUGraphTaskId];

	// Used to construct unique input/output labels, ultimately consumed in graph executor in BuildTaskInput and PostExecute for input/output respectively.
	int InputCount = 0;
	int OutputCount = 0;

	// Add all compute graph task inputs and outputs.
	for (FPCGTaskId GPUTaskId : InCollapsedTasks)
	{
		// First find CPU to GPU edges and wire in the GPU graph node inputs.
		for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[GPUTaskId].Inputs)
		{
			if (InGPUCompatibleTaskIds.Contains(Input.TaskId))
			{
				continue;
			}

			FPCGGraphTaskInput& AddedInput = GPUGraphTask.Inputs.Add_GetRef(Input);

			// TODO is pinless fine with skipping?
			if (AddedInput.DownstreamPin.IsSet())
			{
				const FName VirtualLabel = *FString::Format(TEXT("{0}-VirtualIn{1}"), { AddedInput.DownstreamPin->Label.ToString(), InputCount });
				const bool bIsInputPin = true;
				OutOriginalToVirtualPin.Add({ GPUTaskId, AddedInput.DownstreamPin->Label, bIsInputPin }, VirtualLabel);
				AddedInput.DownstreamPin->Label = VirtualLabel;

				++InputCount;

				if (const UPCGNode* UpstreamNode = InOutCompiledTasks[Input.TaskId].Node)
				{
					if (const UPCGPin* OutputPin = UpstreamNode->GetOutputPin(AddedInput.UpstreamPin->Label))
					{
						OutOutputCPUPinToVirtualPin.Add(OutputPin, VirtualLabel);
					}
				}
			}
		}

		if (!InTaskSuccessors.Contains(GPUTaskId))
		{
			continue;
		}

		// Next consider GPU to CPU edges to wire in the GPU graph node outputs.
		for (FPCGTaskId Successor : InTaskSuccessors[GPUTaskId])
		{
			if (InGPUCompatibleTaskIds.Contains(Successor))
			{
				continue;
			}

			// Rewire inputs of this downstream CPU node to the outputs of the compute graph task.
			FPCGGraphTask& DownstreamCPUNode = InOutCompiledTasks[Successor];

			// Order matters here! We can never reorder inputs as it will impact execution.
			const int InputCountBefore = DownstreamCPUNode.Inputs.Num();
			for (int SuccessorInputIndex = 0; SuccessorInputIndex < InputCountBefore; ++SuccessorInputIndex)
			{
				// Implementation note: we modify the Inputs array in this loop, so don't take a reference to the current element.
				
				// Skip irrelevant edges.
				if (DownstreamCPUNode.Inputs[SuccessorInputIndex].TaskId != GPUTaskId)
				{
					continue;
				}

				// Wire downstream CPU node to compute graph task.
				FPCGGraphTaskInput InputCopy = DownstreamCPUNode.Inputs[SuccessorInputIndex];

				InputCopy.TaskId = InGPUGraphTaskId;

				if (DownstreamCPUNode.Inputs[SuccessorInputIndex].UpstreamPin.IsSet())
				{
					const FNodePin PinKey = { GPUTaskId, InputCopy.UpstreamPin->Label, /*Pin is input*/false };
					if (const FName* FoundVirtualPinLabel = OutOriginalToVirtualPin.Find(PinKey))
					{
						// Wire to the existing virtual output pin.
						InputCopy.UpstreamPin->Label = *FoundVirtualPinLabel;
					}
					else
					{
						const FName VirtualLabel = *FString::Format(TEXT("{0}-VirtualOut{1}"), { InputCopy.UpstreamPin->Label.ToString(), OutputCount });
						OutOriginalToVirtualPin.Add(PinKey, VirtualLabel);

						InputCopy.UpstreamPin->Label = VirtualLabel;

						++OutputCount;
					}
				}

				DownstreamCPUNode.Inputs.Add(MoveTemp(InputCopy));
			}
		}
	}
}

void FPCGGraphCompiler::BuildGPUGraphTask(
	UPCGGraph* InGraph,
	FPCGTaskId InGPUGraphTaskId,
	const TSet<FPCGTaskId>& InCollapsedTasks,
	const TMap<FPCGTaskId, TArray<FPCGTaskId>>& InTaskSuccessors,
	TArray<FPCGGraphTask>& InOutCompiledTasks,
	const FOriginalToVirtualPin& InOriginalToVirtualPin,
	const TMap<const UPCGPin*, FName>& InOutputCPUPinToVirtualPin)
{
	TSharedPtr<FPCGComputeGraphElement> Element = MakeShared<FPCGComputeGraphElement>();
	InOutCompiledTasks[InGPUGraphTaskId].Element = Element;

	const FName GraphName = MakeUniqueObjectName(InGraph, UPCGComputeGraph::StaticClass(), InGraph->GetFName());

	UPCGComputeGraph* ComputeGraph = NewObject<UPCGComputeGraph>(InGraph, GraphName);
	ComputeGraph->OutputCPUPinToInputGPUPinAlias = InOutputCPUPinToVirtualPin;
	Element->Graph.Reset(ComputeGraph);

	// Not incredibly useful for us - DG adds GetComponentSource()->GetComponentClass() object which allows it to bind at execution time by class.
	// But execution code requires it currently.
	Element->Graph->Bindings.Add(UPCGDataBinding::StaticClass());

	// Create data interfaces which allow kernels to read or write data. Each data interface is associated with a node output pin.
	// For CPU->GPU edges, an upload data interface is created. For GPU->CPU edges, a readback data interface is created.
	auto CreateDataInterface = [&InCollapsedTasks, &InOutCompiledTasks, ComputeGraph](FPCGTaskId InTaskId, bool bRequiresReadback, const FPCGPinProperties& InOutputPinProperties) -> UPCGComputeDataInterface*
	{
		const bool bUpstreamIsGPUTask = InCollapsedTasks.Contains(InTaskId);

		EPCGDataType PinType = InOutputPinProperties.AllowedTypes;

		// Dynamically typed pins could have a different type
		const UPCGSettings* Settings = InOutCompiledTasks[InTaskId].Node ? InOutCompiledTasks[InTaskId].Node->GetSettings() : nullptr;
		const UPCGPin* Pin = InOutCompiledTasks[InTaskId].Node ? InOutCompiledTasks[InTaskId].Node->GetOutputPin(InOutputPinProperties.Label) : nullptr;
		if (Settings && Pin)
		{
			PinType = Settings->GetCurrentPinTypes(Pin);
		}

		UPCGComputeDataInterface* DataInterface = nullptr;

		switch (PinType)
		{
		case EPCGDataType::Point:
		case EPCGDataType::PointOrParam:
		{
			UPCGDataCollectionDataInterface* DataInterfacePCGData = nullptr;

			if (bUpstreamIsGPUTask)
			{
				if (bRequiresReadback)
				{
					// GPU -> CPU
					DataInterfacePCGData = NewObject<UPCGDataCollectionReadbackDataInterface>(ComputeGraph);
				}
				else
				{
					// GPU -> GPU
					DataInterfacePCGData = NewObject<UPCGDataCollectionDataInterface>(ComputeGraph);
				}
			}
			else
			{
				// CPU -> GPU
				DataInterfacePCGData = NewObject<UPCGDataCollectionUploadDataInterface>(ComputeGraph);
			}

			check(DataInterfacePCGData);
			DataInterfacePCGData->ProducerSettings = InOutCompiledTasks[InTaskId].Node ? InOutCompiledTasks[InTaskId].Node->GetSettings() : nullptr;

			DataInterface = DataInterfacePCGData;
			break;
		}
		case EPCGDataType::Texture:
		{
			DataInterface = NewObject<UPCGTextureDataInterface>(ComputeGraph);
			break;
		}
		case EPCGDataType::Landscape:
		{
			DataInterface = NewObject<UPCGLandscapeDataInterface>(ComputeGraph);
			break;
		}
		default:
			ensure(false);
			break;
		}

		if (DataInterface)
		{
			DataInterface->SetOutputPin(InOutputPinProperties.Label);
		}

		return DataInterface;
	};

	TMap<TPair</* Node task*/FPCGTaskId, /*Node output pin*/FName>, UPCGComputeDataInterface*> OutputPinDataInterfaces;

	// Create all the output data interfaces.
	for (FPCGTaskId TaskId : InCollapsedTasks)
	{
		// Create DIs for all output pins, because the kernels currently need their outputs to be bound to valid resources.
		if (const UPCGSettings* Settings = InOutCompiledTasks[TaskId].Node ? InOutCompiledTasks[TaskId].Node->GetSettings() : nullptr)
		{
			for (const FPCGPinProperties& OutputPinProperties : Settings->AllOutputPinProperties())
			{
				if (OutputPinDataInterfaces.Contains({ TaskId, OutputPinProperties.Label }))
				{
					ensure(false);
					continue;
				}

				bool bRequiresReadback = false;
				if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(TaskId))
				{
					for (FPCGTaskId Successor : *Successors)
					{
						for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[Successor].Inputs)
						{
							if (Input.UpstreamPin.IsSet() && (*Input.UpstreamPin == OutputPinProperties) && !InCollapsedTasks.Contains(Successor))
							{
								bRequiresReadback = true;
								break;
							}
						}
					}
				}

				if (UPCGComputeDataInterface* OutputDI = CreateDataInterface(TaskId, bRequiresReadback, OutputPinProperties))
				{
					OutputDI->SetOutputPin(OutputPinProperties.Label);
					ComputeGraph->DataInterfaces.Add(OutputDI);
					OutputPinDataInterfaces.Add({ TaskId, OutputPinProperties.Label }, OutputDI);

					for (const FPCGKernelAttributeKey& Key : Settings->GetKernelAttributeKeys())
					{
						if (!ComputeGraph->GlobalAttributeLookupTable.Find(Key))
						{
							ComputeGraph->GlobalAttributeLookupTable.Add(Key, ComputeGraph->GlobalAttributeLookupTable.Num() + PCGComputeConstants::NUM_RESERVED_ATTRS);
						}
					}
				}
			}
		}

		// Create any DIs for upstream CPU nodes.
		for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[TaskId].Inputs)
		{
			// Only deal with upstream CPU tasks.
			if (InCollapsedTasks.Contains(Input.TaskId))
			{
				continue;
			}

			if (!Input.DownstreamPin.IsSet())
			{
				continue;
			}

			if (const UPCGNode* DownstreamNode = InOutCompiledTasks[TaskId].Node)
			{
				if (const UPCGPin* InputPin = DownstreamNode->GetInputPin(Input.DownstreamPin->Label))
				{
					ComputeGraph->PinsReceivingDataFromCPU.Add(InputPin);
				}
			}

			if (!Input.UpstreamPin.IsSet())
			{
				continue;
			}

			if (OutputPinDataInterfaces.Contains({ Input.TaskId, Input.UpstreamPin->Label }))
			{
				// Skip if already created.
				continue;
			}

			if (UPCGComputeDataInterface* OutputDI = CreateDataInterface(Input.TaskId, /*bRequiresReadback=*/false, *Input.UpstreamPin))
			{
				OutputDI->SetOutputPin(Input.UpstreamPin->Label);
				ComputeGraph->DataInterfaces.Add(OutputDI);
				OutputPinDataInterfaces.Add({ Input.TaskId, Input.UpstreamPin->Label }, OutputDI);
			}
		}
	}

	TSet<FPCGTaskId> RemainingTasks = InCollapsedTasks;

	while (!RemainingTasks.IsEmpty())
	{
		// Find a ready task
		FPCGTaskId TaskId = InvalidPCGTaskId;
		for (FPCGTaskId RemainingTask : RemainingTasks)
		{
			// TODO: use 'QueueSuccessors' pattern rather than brute force searching for ready tasks
			bool bReady = true;
			for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[RemainingTask].Inputs)
			{
				if (RemainingTasks.Contains(Input.TaskId))
				{
					bReady = false;
					break;
				}
			}

			if (bReady)
			{
				TaskId = RemainingTask;
				break;
			}
		}

		if (TaskId == InvalidPCGTaskId)
		{
			// Tasks in RemainingTasks are unreachable?
			ensure(false);
			break;
		}

		RemainingTasks.Remove(TaskId);

		const UPCGNode* Node = InOutCompiledTasks[TaskId].Node;

		const UPCGCustomHLSLSettings* Settings = Cast<UPCGCustomHLSLSettings>(Node ? Node->GetSettings() : nullptr);
		check(Settings && Settings->bEnabled && Settings->ShouldExecuteOnGPU());

		// For every usage of a DI, get the original (non-aliased) pin label.
		TMap<TPair<FPCGTaskId, UComputeDataInterface*>, FName> DataInterfaceUsageToPinLabel;

		TArray<int> InputDataInterfaceIndices;
		TArray<int> OutputDataInterfaceIndices;
		InputDataInterfaceIndices.Reserve(Settings->InputPinProperties().Num());
		OutputDataInterfaceIndices.Reserve(Settings->OutputPinProperties().Num());

		// Add DIs (PCG -> CF transcoding).

		for (const FPCGGraphTaskInput& Input : InOutCompiledTasks[TaskId].Inputs)
		{
			if (!Input.UpstreamPin.IsSet())
			{
				// Execution-only dependencies not supported currently. Unclear if this should ever be supported for GPU graphs.
				// Writes followed by reads will be protected via barriers added by RDG.
				continue;
			}

			UPCGComputeDataInterface* UpstreamDI = nullptr;
			if (UPCGComputeDataInterface** FoundUpstreamDI = OutputPinDataInterfaces.Find({ Input.TaskId, Input.UpstreamPin->Label }))
			{
				UpstreamDI = *FoundUpstreamDI;
			}

			if (!UpstreamDI)
			{
				ensure(false);
				continue;
			}

			const int Index = ComputeGraph->DataInterfaces.Find(UpstreamDI);
			if (Index == INDEX_NONE)
			{
				ensure(false);
				continue;
			}

			InputDataInterfaceIndices.Add(Index);

			FName DownstreamInputPinLabel = Input.DownstreamPin->Label;

			DataInterfaceUsageToPinLabel.Add({ TaskId, UpstreamDI }, DownstreamInputPinLabel);

			const bool bIsInputPin = true;
			UpstreamDI->AddDownstreamInputPin(DownstreamInputPinLabel, InOriginalToVirtualPin.Find({ TaskId, DownstreamInputPinLabel, bIsInputPin }));
		}

		// Always create a DI for every output pin, so kernel always has something to write to.
		for (const FPCGPinProperties& OutputPinProperties : Settings->AllOutputPinProperties())
		{
			UPCGComputeDataInterface** FoundDI = OutputPinDataInterfaces.Find({ TaskId, OutputPinProperties.Label });
			if (!ensure(FoundDI) || !ensure(*FoundDI))
			{
				continue;
			}

			const int Index = ComputeGraph->DataInterfaces.Find(*FoundDI);
			if (Index == INDEX_NONE)
			{
				ensure(false);
				continue;
			}

			OutputDataInterfaceIndices.Add(Index);
			DataInterfaceUsageToPinLabel.Add({ TaskId, *FoundDI }, OutputPinProperties.Label);
		}

		// Make sure every downstream input pin is registered with the upstream DI.
		if (const TArray<FPCGTaskId>* Successors = InTaskSuccessors.Find(TaskId))
		{
			for (FPCGTaskId Successor : *Successors)
			{
				for (const FPCGGraphTaskInput& SuccessorInput : InOutCompiledTasks[Successor].Inputs)
				{
					if (SuccessorInput.TaskId != TaskId)
					{
						continue;
					}

					if (!SuccessorInput.UpstreamPin.IsSet() || !SuccessorInput.DownstreamPin.IsSet())
					{
						continue;
					}

					FName OutputPinLabel = SuccessorInput.UpstreamPin->Label;

					// DIs for output pins should have all been created.
					UPCGComputeDataInterface** FoundOutputDI = OutputPinDataInterfaces.Find({ TaskId, OutputPinLabel });
					if (!ensure(FoundOutputDI) || !ensure(*FoundOutputDI))
					{
						continue;
					}
					
					// If the map has an entry for this task and output pin label, then its a GPU->CPU readback.
					const bool bIsInputPin = false;
					(*FoundOutputDI)->SetOutputPin(OutputPinLabel, InOriginalToVirtualPin.Find({ TaskId, OutputPinLabel, bIsInputPin }));
				}
			}
		}

		UPCGCustomKernelDataInterface* KernelDI = NewObject<UPCGCustomKernelDataInterface>(ComputeGraph);
		KernelDI->Settings = Settings;
		const int KernelDIIndex = ComputeGraph->DataInterfaces.Num();
		ComputeGraph->DataInterfaces.Add(KernelDI);

		// TODO add graph data interface (graph params). Reference: UOptimusGraphDataInterface.
		//Element->Graph->DataInterfaces.Add(NewObject< UPCGDataCollectionDataInterface>(InGraph));
		//Element->Graph->DataInterfaceToBinding.Add(0);

		// TODO once we support cooking for different platforms/configs, don't create the interface if logging is not present.
		int DebugDIIndex = INDEX_NONE;
		if (Settings->bPrintShaderDebugValues)
		{
			UPCGDebugDataInterface* DebugDI = NewObject<UPCGDebugDataInterface>(ComputeGraph);
			DebugDI->SetDebugBufferSize(Settings->DebugBufferSize);

			DebugDIIndex = ComputeGraph->DataInterfaces.Num();
			ComputeGraph->DataInterfaces.Add(DebugDI);
		}

		// Now that all data interfaces added, create the (trivial) binding mapping. All map to primary binding, index 0.
		ComputeGraph->DataInterfaceToBinding.SetNumZeroed(ComputeGraph->DataInterfaces.Num());

		struct FInterfaceBinding
		{
			const UComputeDataInterface* DataInterface;
			int32 DataInterfaceBindingIndex;
			FString BindingFunctionName;
			FString BindingFunctionNamespace;
		};

		struct FKernelWithDataBindings
		{
			UComputeKernel* Kernel;
			TArray<FInterfaceBinding> InputDataBindings;
			TArray<FInterfaceBinding> OutputDataBindings;
		};

		FKernelWithDataBindings KernelWithBindings;

		KernelWithBindings.Kernel = NewObject<UComputeKernel>(ComputeGraph);
		const int KernelIndex = ComputeGraph->KernelInvocations.Num();
		ComputeGraph->KernelInvocations.Add(KernelWithBindings.Kernel);
		ComputeGraph->KernelToNode.Add(Node);
		
		auto SetupAllInputBindings = [&KernelWithBindings, ComputeGraph](int InDataInterfaceIndex)
		{
			const UComputeDataInterface* DataInterface = ComputeGraph->DataInterfaces[InDataInterfaceIndex];
			TArray<FShaderFunctionDefinition> Functions;
			DataInterface->GetSupportedInputs(Functions);

			for (int FuncIndex = 0; FuncIndex < Functions.Num(); ++FuncIndex)
			{
				FInterfaceBinding& Binding = KernelWithBindings.InputDataBindings.Emplace_GetRef();
				Binding.DataInterface = DataInterface;
				Binding.BindingFunctionName = Functions[FuncIndex].Name;
				Binding.BindingFunctionNamespace = TEXT("");
				Binding.DataInterfaceBindingIndex = FuncIndex;
			}
		};

		auto SetupAllOutputBindings = [&KernelWithBindings, ComputeGraph](int InDataInterfaceIndex)
		{
			const UComputeDataInterface* DataInterface = ComputeGraph->DataInterfaces[InDataInterfaceIndex];
			TArray<FShaderFunctionDefinition> Functions;
			DataInterface->GetSupportedOutputs(Functions);

			for (int FuncIndex = 0; FuncIndex < Functions.Num(); ++FuncIndex)
			{
				FInterfaceBinding& Binding = KernelWithBindings.OutputDataBindings.Emplace_GetRef();
				Binding.DataInterface = DataInterface;
				Binding.BindingFunctionName = Functions[FuncIndex].Name;
				Binding.BindingFunctionNamespace = TEXT("");
				Binding.DataInterfaceBindingIndex = FuncIndex;
			}
		};

		// Bind data interfaces.
		for (int InputDataInterfaceIndex : InputDataInterfaceIndices)
		{
			SetupAllInputBindings(InputDataInterfaceIndex);
		}

		SetupAllInputBindings(KernelDIIndex);

		for (int OutputDataInterfaceIndex : OutputDataInterfaceIndices)
		{
			SetupAllOutputBindings(OutputDataInterfaceIndex);
		}

		if (DebugDIIndex != INDEX_NONE)
		{
			SetupAllOutputBindings(DebugDIIndex);
		}

		{
			UPCGComputeKernelSource* KernelSource = NewObject<UPCGComputeKernelSource>(KernelWithBindings.Kernel); // is outer to kernel fine?
			KernelWithBindings.Kernel->KernelSource = KernelSource;
			KernelSource->EntryPoint = Settings->GetKernelEntryPoint();
			KernelSource->GroupSize = Settings->GetThreadGroupSize();

			KernelSource->SetSource(Settings->GetCookedKernelSource(ComputeGraph->GlobalAttributeLookupTable));

#if WITH_EDITOR
			if (PCGGraphCompiler::CVarEnableGPUDebugging.GetValueOnAnyThread())
			{
				UE_LOG(LogPCG, Warning, TEXT("ATTRIBUTE LOOK-UP TABLE [%s]"), *Settings->GetDefaultNodeTitle().ToString());

				for (const TPair<FPCGKernelAttributeKey, int32 /* Attribute Index */>& Pair : ComputeGraph->GlobalAttributeLookupTable)
				{
					const FString TypeString = UEnum::GetValueAsString(Pair.Key.Type);
					const FString NameString = Pair.Key.Name.ToString();
					const FString IndexString = FString::FromInt(Pair.Value);

					UE_LOG(LogPCG, Warning, TEXT("%s: %s (%s)"), *IndexString, *NameString, *TypeString);
				}
			}
#endif

			// Add functions for external inputs/outputs which must be fulfilled by DIs
			for (FInterfaceBinding& Binding : KernelWithBindings.InputDataBindings)
			{
				TArray<FShaderFunctionDefinition> Functions;
				Binding.DataInterface->GetSupportedInputs(Functions);
				check(Functions.IsValidIndex(Binding.DataInterfaceBindingIndex));

				FShaderFunctionDefinition FuncDef = Functions[Binding.DataInterfaceBindingIndex];
				for (FShaderParamTypeDefinition& ParamType : FuncDef.ParamTypes)
				{
					// Making sure parameter has type declaration generated
					ParamType.ResetTypeDeclaration();
				}

				KernelSource->ExternalInputs.Emplace(FuncDef);
			}

			for (FInterfaceBinding& Binding : KernelWithBindings.OutputDataBindings)
			{
				TArray<FShaderFunctionDefinition> Functions;
				Binding.DataInterface->GetSupportedOutputs(Functions);
				check(Functions.IsValidIndex(Binding.DataInterfaceBindingIndex));

				FShaderFunctionDefinition FuncDef = Functions[Binding.DataInterfaceBindingIndex];
				for (FShaderParamTypeDefinition& ParamType : FuncDef.ParamTypes)
				{
					// Making sure parameter has type declaration generated
					ParamType.ResetTypeDeclaration();
				}

				KernelSource->ExternalOutputs.Emplace(FuncDef);
			}
		}

		auto AddAllEdgesForKernel = [&KernelWithBindings, ComputeGraph, TaskId, &DataInterfaceUsageToPinLabel](int32 InKernelIndex, bool bInEdgesAreInputs)
		{
			TArray<FInterfaceBinding>& Bindings = bInEdgesAreInputs ? KernelWithBindings.InputDataBindings : KernelWithBindings.OutputDataBindings;

			// Add all graph edges for bindings. This is somewhat odd but likely what we'll do vs the more finegrained interface
			// definitions in optimus, but we need to see.
			for (int BindingIndex = 0; BindingIndex < Bindings.Num(); ++BindingIndex)
			{
				FInterfaceBinding& Binding = Bindings[BindingIndex];

				FComputeGraphEdge& Edge = ComputeGraph->GraphEdges.Emplace_GetRef();
				Edge.KernelIndex = InKernelIndex;
				Edge.KernelBindingIndex = BindingIndex;
				Edge.DataInterfaceIndex = ComputeGraph->DataInterfaces.IndexOfByPredicate([&Binding](const UComputeDataInterface* In) { return Binding.DataInterface == In; });
				check(Edge.DataInterfaceIndex != INDEX_NONE);
				Edge.DataInterfaceBindingIndex = Binding.DataInterfaceBindingIndex;
				Edge.bKernelInput = bInEdgesAreInputs;

				UComputeDataInterface* DataInterface = ComputeGraph->DataInterfaces[Edge.DataInterfaceIndex];
				check(DataInterface);

				if (FName* PinLabel = DataInterfaceUsageToPinLabel.Find({ TaskId, DataInterface }))
				{
					TArray<FShaderFunctionDefinition> DataInterfaceFunctions;
					if (bInEdgesAreInputs)
					{
						DataInterface->GetSupportedInputs(DataInterfaceFunctions);
					}
					else
					{
						DataInterface->GetSupportedOutputs(DataInterfaceFunctions);
					}

					Edge.BindingFunctionNameOverride = FString::Format(
						TEXT("{0}_{1}"),
						{ PinLabel->ToString(), DataInterfaceFunctions[Edge.DataInterfaceBindingIndex].Name }
					);
				}
			}
		};

		AddAllEdgesForKernel(KernelIndex, /*bInEdgesAreInputs=*/true);
		AddAllEdgesForKernel(KernelIndex, /*bInEdgesAreInputs=*/false);
	}

	// Register all virtual pin aliases with the corresponding pins for downstream usage.
	for (const TPair<TTuple<FPCGTaskId, FName, bool>, FName>& Mapping : InOriginalToVirtualPin)
	{
		const bool bIsInputPin = Mapping.Get<0>().Get<2>();
		if (bIsInputPin)
		{
			const FPCGTaskId TaskId = Mapping.Get<0>().Get<0>();

			if (const UPCGNode* Node = InOutCompiledTasks[TaskId].Node)
			{
				const FName& OriginalLabel = Mapping.Get<0>().Get<1>();

				if (const UPCGPin* Pin = Node->GetInputPin(OriginalLabel))
				{
					const FName& VirtualLabel = Mapping.Get<1>();
					ComputeGraph->InputPinLabelAliases.FindOrAdd(Pin) = VirtualLabel;
				}
			}
		}
	}

	// Kick off shader compilation (if needed).
	ComputeGraph->UpdateResources();
}

void FPCGGraphCompiler::CreateGPUNodes(UPCGGraph* InGraph, TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	TSet<FPCGTaskId> GPUCompatibleTaskIds;
	GPUCompatibleTaskIds.Reserve(InOutCompiledTasks.Num());
	for (FPCGTaskId TaskId = 0; TaskId < InOutCompiledTasks.Num(); ++TaskId)
	{
		const UPCGNode* Node = InOutCompiledTasks[TaskId].Node;
		const UPCGSettings* Settings = Node ? Node->GetSettings() : nullptr;
		if (Settings && Settings->ShouldExecuteOnGPU() && Settings->bEnabled)
		{
			GPUCompatibleTaskIds.Add(TaskId);
		}
	}

	if (GPUCompatibleTaskIds.IsEmpty())
	{
		// Nothing to do for this graph.
		return;
	}

	TMap<FPCGTaskId, TArray<FPCGTaskId>> TaskSuccessors;
	TaskSuccessors.Reserve(InOutCompiledTasks.Num());
	for (FPCGTaskId TaskId = 0; TaskId < InOutCompiledTasks.Num(); ++TaskId)
	{
		for (int InputIndex = 0; InputIndex < InOutCompiledTasks[TaskId].Inputs.Num(); ++InputIndex)
		{
			TaskSuccessors.FindOrAdd(InOutCompiledTasks[TaskId].Inputs[InputIndex].TaskId).Add(TaskId);
		}
	}

	// For input pins at CPU -> GPU boundary, inject gather elements to pre-combine data on CPU side
	// before passing to GPU.
	CreateGatherTasksAtGPUInputs(GPUCompatibleTaskIds, InOutCompiledTasks);

	TArray<TSet<FPCGTaskId>> NodeSubsetsToConvertToCFGraph;
	CollectGPUNodeSubsets(InOutCompiledTasks, TaskSuccessors, GPUCompatibleTaskIds, NodeSubsetsToConvertToCFGraph);

	// Do actual collapsing now, one subset at a time. Each collapse will do all fixup of task ids? That will invalidate
	// ids in NodeSubsetsToConvertToCFGraph, so may need remap table. But can ignore this for now.
	for (TSet<FPCGTaskId>& NodeSubsetToConvertToCFGraph : NodeSubsetsToConvertToCFGraph)
	{
		if (NodeSubsetToConvertToCFGraph.IsEmpty())
		{
			ensure(false);
			continue;
		}

		// Add a new compute graph task. Then kill the original GPU tasks.
		const FPCGTaskId ComputeGraphTaskId = InOutCompiledTasks.Num();
		FPCGGraphTask& ComputeGraphTask = InOutCompiledTasks.Emplace_GetRef();
		ComputeGraphTask.NodeId = ComputeGraphTaskId;

		// All nodes in subset will be from same stack/parent, so assign from any.
		for (FPCGTaskId GPUTaskId : NodeSubsetToConvertToCFGraph)
		{
			ComputeGraphTask.ParentId = InOutCompiledTasks[GPUTaskId].ParentId;
			ComputeGraphTask.StackIndex = InOutCompiledTasks[GPUTaskId].StackIndex;
			break;
		}

		// Mapping from task ID & pin label to a virtual pin label. Compute graphs are executed within a generated element,
		// and the input and output pins of this element must have unique virtual pin labels so that we can parse the data that
		// PCG provides through the input data collection correctly, and route the output data to the downstream pins correctly.
		FOriginalToVirtualPin OriginalToVirtualPin;

		TMap<const UPCGPin*, FName> OutputCPUPinToVirtualPin;

		// Wire in the compute graph task, side by side with the individual GPU tasks, which will be culled below.
		WireGPUGraphNode(
			ComputeGraphTaskId,
			NodeSubsetToConvertToCFGraph,
			GPUCompatibleTaskIds,
			InOutCompiledTasks,
			TaskSuccessors,
			OriginalToVirtualPin,
			OutputCPUPinToVirtualPin);

		// Generate a compute graph from all of the individual GPU tasks.
		BuildGPUGraphTask(
			InGraph,
			ComputeGraphTaskId,
			NodeSubsetToConvertToCFGraph,
			TaskSuccessors,
			InOutCompiledTasks,
			OriginalToVirtualPin,
			OutputCPUPinToVirtualPin);
	}

	// Now cull all the GPU compatible nodes. The compute graph task are already wired in so we're fine to just delete.
	CullTasks(InOutCompiledTasks, /*bAddPassthroughWires=*/false, [&GPUCompatibleTaskIds](const FPCGGraphTask& InTask) { return GPUCompatibleTaskIds.Contains(InTask.NodeId); });
}
