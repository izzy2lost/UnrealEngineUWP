// Copyright Epic Games, Inc. All Rights Reserved.


#include "AnimNextRuntimeTest.h"

#include "TraitCore/TraitReader.h"
#include "Module/AnimNextModule.h"
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

	bool FTestUtils::LoadFromArchiveBuffer(UAnimNextModule& Module, TArray<FNodeHandle>& NodeHandles, const TArray<uint8>& SharedDataArchiveBuffer)
	{
		FAnimNextGraphEvaluatorExecuteDefinition ExecuteDefinition;
		ExecuteDefinition.Hash = 0;
		ExecuteDefinition.MethodName = TEXT("Execute_0");

		// Manually add our entry point since we didn't go through a full RigVM graph
		Module.EntryPoints.Reset();

		FAnimNextGraphEntryPoint& EntryPoint = Module.EntryPoints.AddDefaulted_GetRef();
		EntryPoint.EntryPointName = Module.GetDefaultEntryPoint();
		EntryPoint.RootTraitHandle = FAnimNextEntryPointHandle(NodeHandles[0]);
		Module.ExecuteDefinition = ExecuteDefinition;
		Module.SharedDataArchiveBuffer = SharedDataArchiveBuffer;
		Module.GraphReferencedObjects.Empty();

		// Reconstruct our graph shared data
		FMemoryReader GraphSharedDataArchive(SharedDataArchiveBuffer);
		FTraitReader TraitReader(Module.GraphReferencedObjects, GraphSharedDataArchive);

		const FTraitReader::EErrorState ErrorState = TraitReader.ReadGraph(Module.SharedDataBuffer);
		if (ErrorState == FTraitReader::EErrorState::None)
		{
			Module.ResolvedRootTraitHandles.Add(Module.GetDefaultEntryPoint(), TraitReader.ResolveEntryPointHandle(Module.EntryPoints[0].RootTraitHandle));

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
			Module.SharedDataBuffer.Empty(0);
			Module.ResolvedRootTraitHandles.Add(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, FAnimNextTraitHandle());
			return false;
		}
	}
}
#endif
