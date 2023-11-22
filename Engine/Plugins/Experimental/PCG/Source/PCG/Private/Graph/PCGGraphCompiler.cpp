// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGGraphCompiler.h"

#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Elements/PCGHiGenGridSize.h"
#include "Elements/PCGReroute.h"
#include "Graph/PCGGraphExecutor.h"

#include "Misc/ScopeRWLock.h"

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

		// Only catch immediate infinite recursion.
		// TODO: Add a better mechanism for detecting more complex cyclic recursions
		// Like Graph A has subgraph node with Graph B, and Graph B has a subgraph node with Graph A (A -> B -> A)
		// or A -> B -> C -> A, etc...
		// NOTE (for the person that would work on it), keeping a stack of all the subgraphs is not enough, as we could already have compiled graph
		// A, with an inclusion to graph B, but B has not yet a subgraph to A. When adding subgraph node to A in B, we will only recompile B, since A is already
		// compiled and cached... We need to store a subgraph dependency chain to detect those more complex cases.
		if (Subgraph == InGraph)
		{
			UE_LOG(LogPCG, Error, TEXT("[FPCGGraphCompiler::CompileGraph] %s cannot include itself as a subgraph, subgraph will not be executed."), *InGraph->GetName());
			return TArray<FPCGGraphTask>();
		}

		const bool bIsNonDynamicAndNonDisabledSubgraphNode = (SubgraphNode && Subgraph && SubgraphSettings && !SubgraphSettings->IsDynamicGraph() && SubgraphSettings->bEnabled);
		if (bIsNonDynamicAndNonDisabledSubgraphNode)
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
						PreTask.Inputs.Emplace(IdMapping[InboundEdge->InputPin->Node], InboundEdge->InputPin, InboundEdge->OutputPin);
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
				InputNodeTask->Inputs.Emplace(PreId, nullptr, nullptr);
			}

			// Hook nodes to the PreTask if they require so.
			// Only do it for nodes that are directly under the subgraph, not in subsequent subgraphs.
			for (FPCGGraphTask& Subtask : Subtasks)
			{
				if (!Subtask.Node || Subtask.Node->GetOuter() != Subgraph)
				{
					continue;
				}

				const UPCGSettings* Settings = Subtask.Node->GetSettings();
				if (Settings && Settings->ShouldHookToPreTask())
				{
					Subtask.Inputs.Emplace(PreId, nullptr, nullptr);
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
			PostTask.Element = MakeShared<FPCGTrivialElement>();

			// Add subgraph output node task as input to the post-task
			if (OutputNodeTask)
			{
				PostTask.Inputs.Emplace(OutputNodeTask->NodeId, nullptr, nullptr);
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
						Task.Inputs.Emplace(*InboundId, InboundEdge->InputPin, InboundEdge->OutputPin); 
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
			if (!TaskInput.InPin)
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
				if (!ensure(CurrentStack->CreateStackFramePath(ResourceKey, TaskInput.InPin->Node, TaskInput.InPin)))
				{
					continue;
				}

				// Build task & element to hold the operation to perform
				FPCGGraphTask& LinkTask = InOutCompiledTasks.Emplace_GetRef();
				LinkTask.NodeId = InOutCompiledTasks.Num() - 1;
				LinkTask.StackIndex = InOutCompiledTasks[TaskId].StackIndex;

				LinkTask.Inputs.Emplace(TaskInput.TaskId, TaskInput.InPin, nullptr, /*bConsumeInputData=*/true);

				const EPCGHiGenGrid FromGrid = InOutTaskGenerationGrid[TaskInput.TaskId];
				const EPCGHiGenGrid ToGrid = InOutTaskGenerationGrid[TaskId];

				// This lambda runs at execution time and attempts to retrieve the data from a larger grid. Capture by value is intentional.
				auto GridLinkageOperation = [FromGrid, ToGrid, ResourceKey, OutputPinLabel = TaskInput.InPin->Properties.Label,
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
				LinkTask.Element = MakeShared<FPCGGenericElement>(GridLinkageOperation, ContextAllocator);

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
		Grid = GridSizeSettings->GetGrid();
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

bool FPCGGraphCompiler::CalculateActiveRecursive(FPCGTaskId InTaskId, const TArray<FPCGGraphTask>& InCompiledTasks, TMap<int32, bool>& InTaskIdToActiveFlag)
{
	if (const bool* bEntry = InTaskIdToActiveFlag.Find(InTaskId))
	{
		return *bEntry;
	}

	bool bAnyInputActive = false;

	for (FPCGGraphTaskInput Input : InCompiledTasks[InTaskId].Inputs)
	{
		// Default to tasks being active unless proved otherwise.
		bool bInputActive = true;

		// If we are connected to an upstream branch node, evaluate if the output pin is active.
		const UPCGNode* UpstreamNode = InCompiledTasks[Input.TaskId].Node;
		if (const UPCGSettings* UpstreamSettings = UpstreamNode ? UpstreamNode->GetSettings() : nullptr)
		{
			bInputActive &= UpstreamSettings->IsPinStaticallyActive(Input.InPin->Properties.Label);
		}

		if (bInputActive)
		{
			bInputActive &= CalculateActiveRecursive(Input.TaskId, InCompiledTasks, InTaskIdToActiveFlag);
		}

		if (bInputActive)
		{
			bAnyInputActive = true;
			break;
		}
	}

	const bool bActive = bAnyInputActive || InCompiledTasks[InTaskId].Inputs.IsEmpty();
	InTaskIdToActiveFlag.Add(InTaskId, bActive);
	return bActive;
}

void FPCGGraphCompiler::CullTasksStaticBranchNodes(TArray<FPCGGraphTask>& InOutCompiledTasks)
{
	if (InOutCompiledTasks.IsEmpty())
	{
		return;
	}

	TMap<int32, bool> NodeIdToActiveFlag;
	// First task is input node task which is active
	NodeIdToActiveFlag.Add(InOutCompiledTasks[0].NodeId, true);

	for (int32 i = 1; i < InOutCompiledTasks.Num(); ++i)
	{
		CalculateActiveRecursive(InOutCompiledTasks[i].NodeId, InOutCompiledTasks, NodeIdToActiveFlag);
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

	TArray<int32> TaskRemapping;
	TaskRemapping.SetNumUninitialized(InOutCompiledTasks.Num());

	// Mark culled tasks by remapping to INDEX_NONE. First task is input task and is never culled.
	TaskRemapping[0] = 0;
	for (int32 TaskIndex = 1; TaskIndex < InOutCompiledTasks.Num(); ++TaskIndex)
	{
		TaskRemapping[TaskIndex] = CullTask(InOutCompiledTasks[TaskIndex]) ? INDEX_NONE : 0;
	}

	// Optionally add wires that bypass culled nodes.
	if (bAddPassthroughWires)
	{
		for (int32 TaskIndex = 1; TaskIndex < InOutCompiledTasks.Num(); ++TaskIndex)
		{
			FPCGGraphTask& Task = InOutCompiledTasks[TaskIndex];
			if (!Task.Node || Task.Node->GetInputPins().IsEmpty())
			{
				continue;
			}

			const int32 InputNumBefore = Task.Inputs.Num();
			for (int32 InputIndex = 0; InputIndex < InputNumBefore; ++InputIndex)
			{
				const int32 InputTaskId = Task.Inputs[InputIndex].TaskId;

				// Is node culled?
				if (TaskRemapping[InputTaskId] == INDEX_NONE)
				{
					const FPCGGraphTask& InputTask = InOutCompiledTasks[InputTaskId];
					if (InputTask.Node && InputTask.Node->GetInputPins().Num() > 1)
					{
						ensureMsgf(false, TEXT("Task culling currently only supports nodes with a single input pin which are trivial to unwire."));
						continue;
					}

					// Upstream node was culled. Wire up the inputs of the culled node to this node.
					for (int32 InputInputIndex = 0; InputInputIndex < InputTask.Inputs.Num(); ++InputInputIndex)
					{
						FPCGGraphTaskInput& NewInput = Task.Inputs.Add_GetRef(InputTask.Inputs[InputInputIndex]);
						NewInput.OutPin = Task.Inputs[InputIndex].OutPin;
					}
				}
			}
		}
	}

	// Remove all culled tasks by compacting the task array. Never cull first (Input) task.
	int32 WriteIndex = 1;
	int32 ReadIndex = 1;
	while (ReadIndex < InOutCompiledTasks.Num())
	{
		// If not culled, then move the task to it's final remapped position in the task array.
		if (TaskRemapping[ReadIndex] != INDEX_NONE)
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
		for (int32 InputIndex = Task.Inputs.Num() - 1; InputIndex >= 0; --InputIndex)
		{
			const int32 InputTaskId = Task.Inputs[InputIndex].TaskId;
			const int32 Remap = TaskRemapping[InputTaskId];
			if (Remap != INDEX_NONE)
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
			const int32 RemappedParentId = TaskRemapping[Task.ParentId];
			// Parent task should not have been culled.
			ensure(RemappedParentId != INDEX_NONE);

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

	// Build from non-top tasks
	FPCGStackContext StackContext;
	TArray<FPCGGraphTask> CompiledTasks = GetCompiledTasks(InGraph, GenerationGridSize, StackContext, /*bIsTopGraph=*/false);

	// Check that the compilation was valid
	if (CompiledTasks.Num() == 0)
	{
		return;
	}

	// Remove reroute nodes before execution grid setup, as grid linkages need final nodes to connect from/to.
	CullTasks(CompiledTasks, /*bAddPassthroughWires=*/true, [](const FPCGGraphTask& InTask) { return InTask.Node && Cast<UPCGRerouteSettings>(InTask.Node->GetSettings()); });

	// Cull inactive branches downstream of branch nodes with static selection values.
	CullTasksStaticBranchNodes(CompiledTasks);

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

	// Post culling - remove any stacks that are no longer part of execution. Besides being tidy this also helps
	// debug tools discern which stacks were executed or not.
	PostCullStackCleanup(CompiledTasks, StackContext);

	const int TaskNum = CompiledTasks.Num();
	const FPCGTaskId PreExecuteTaskId = FPCGTaskId(TaskNum);
	const FPCGTaskId PostExecuteTaskId = PreExecuteTaskId + 1;

	FPCGGraphTask& PreExecuteTask = CompiledTasks.Emplace_GetRef();
	PreExecuteTask.Element = MakeShared<FPCGTrivialElement>();
	PreExecuteTask.NodeId = PreExecuteTaskId;

	for (int TaskIndex = 0; TaskIndex < TaskNum; ++TaskIndex)
	{
		FPCGGraphTask& Task = CompiledTasks[TaskIndex];
		if (Task.Inputs.IsEmpty())
		{
			Task.Inputs.Emplace(PreExecuteTaskId, nullptr, nullptr);
		}

		Task.CompiledTaskId = Task.NodeId;
	}

	FPCGGraphTask& PostExecuteTask = CompiledTasks.Emplace_GetRef();
	PostExecuteTask.Element = MakeShared<FPCGTrivialElement>();
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
			PostExecuteTask.Inputs.Emplace(Task.NodeId, nullptr, nullptr, bProvideData);
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

void FPCGGraphCompiler::ClearCache()
{
	FWriteScopeLock Lock(GraphToTaskMapLock);
	GraphToTaskMap.Reset();
	GraphToStackContext.Reset();
	TopGraphToTaskMap.Reset();
	TopGraphToStackContextMap.Reset();
}

#if WITH_EDITOR
void FPCGGraphCompiler::NotifyGraphChanged(UPCGGraph* InGraph)
{
	if (InGraph)
	{
		RemoveFromCache(InGraph);
	}
}

void FPCGGraphCompiler::RemoveFromCache(UPCGGraph* InGraph)
{
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
