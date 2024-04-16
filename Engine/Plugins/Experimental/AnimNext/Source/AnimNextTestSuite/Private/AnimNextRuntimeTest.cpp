// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNextRuntimeTest.h"

#include "TraitCore/TraitReader.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UE::AnimNext
{
	FScopedClearNodeTemplateRegistry::FScopedClearNodeTemplateRegistry()
	{
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();
		Swap(Registry, TmpRegistry);
	}

	FScopedClearNodeTemplateRegistry::~FScopedClearNodeTemplateRegistry()
	{
		FNodeTemplateRegistry& Registry = FNodeTemplateRegistry::Get();
		Swap(Registry, TmpRegistry);
	}

	bool FTestUtils::LoadFromArchiveBuffer(UAnimNextGraph& Graph, TArray<FNodeHandle>& NodeHandles, const TArray<uint8>& SharedDataArchiveBuffer)
	{
		FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;
		ExecuteDefinition.Hash = 0;
		ExecuteDefinition.MethodName = TEXT("Execute_0");

		// Manually add our entry point since we didn't go through a full RigVM graph
		Graph.EntryPoints.Reset();

		FAnimNextGraphEntryPoint& EntryPoint = Graph.EntryPoints.AddDefaulted_GetRef();
		EntryPoint.EntryPointName = Graph.GetDefaultEntryPoint();
		EntryPoint.RootTraitHandle = FAnimNextEntryPointHandle(NodeHandles[0]);
		Graph.ExecuteDefinition = ExecuteDefinition;
		Graph.SharedDataArchiveBuffer = SharedDataArchiveBuffer;
		Graph.GraphReferencedObjects.Empty();

		// Reconstruct our graph shared data
		FMemoryReader GraphSharedDataArchive(SharedDataArchiveBuffer);
		FTraitReader TraitReader(Graph.GraphReferencedObjects, GraphSharedDataArchive);

		const FTraitReader::EErrorState ErrorState = TraitReader.ReadGraph(Graph.SharedDataBuffer);
		if (ErrorState == FTraitReader::EErrorState::None)
		{
			Graph.ResolvedRootTraitHandles.Add(Graph.GetDefaultEntryPoint(), TraitReader.ResolveEntryPointHandle(Graph.EntryPoints[0].RootTraitHandle));

			for (FNodeHandle& NodeHandle : NodeHandles)
			{
				NodeHandle = TraitReader.ResolveNodeHandle(NodeHandle);
			}

			// Make sure our execute method is registered
			FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(ExecuteDefinition);
			return true;
		}
		else
		{
			Graph.SharedDataBuffer.Empty(0);
			Graph.ResolvedRootTraitHandles.Add(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, FAnimNextTraitHandle());
			return false;
		}
	}
}
#endif
