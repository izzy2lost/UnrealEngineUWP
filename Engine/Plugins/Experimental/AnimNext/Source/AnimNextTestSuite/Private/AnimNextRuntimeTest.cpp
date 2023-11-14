// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNextRuntimeTest.h"

#include "DecoratorBase/DecoratorReader.h"
#include "Graph/AnimNextGraph.h"
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

		Graph.ExecuteDefinition = ExecuteDefinition;
		Graph.SharedDataArchiveBuffer = SharedDataArchiveBuffer;
		Graph.GraphReferencedObjects.Empty();
		Graph.RootDecoratorHandle = FAnimNextEntryPointHandle(NodeHandles[0]);

		// Reconstruct our graph shared data
		FMemoryReader GraphSharedDataArchive(SharedDataArchiveBuffer);
		FDecoratorReader DecoratorReader(Graph.GraphReferencedObjects, GraphSharedDataArchive);

		const FDecoratorReader::EErrorState ErrorState = DecoratorReader.ReadGraph(Graph.SharedDataBuffer);
		if (ErrorState == FDecoratorReader::EErrorState::None)
		{
			Graph.ResolvedRootDecoratorHandle = DecoratorReader.ResolveEntryPointHandle(Graph.RootDecoratorHandle);

			for (FNodeHandle& NodeHandle : NodeHandles)
			{
				NodeHandle = DecoratorReader.ResolveNodeHandle(NodeHandle);
			}

			// Make sure our execute method is registered
			FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(ExecuteDefinition);
			return true;
		}
		else
		{
			Graph.SharedDataBuffer.Empty(0);
			Graph.ResolvedRootDecoratorHandle = FAnimNextDecoratorHandle();
			return false;
		}
	}
}
#endif
