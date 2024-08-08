// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowToolRegistry.h"
#include "Misc/LazySingleton.h"
#include "Framework/Commands/UICommandInfo.h"

namespace Dataflow
{

	FDataflowToolRegistry& FDataflowToolRegistry::Get()
	{
		return TLazySingleton<FDataflowToolRegistry>::Get();
	}

	void FDataflowToolRegistry::TearDown()
	{
		TLazySingleton<FDataflowToolRegistry>::TearDown();
	}

	void FDataflowToolRegistry::AddNodeToToolMapping(const FName& NodeName, TObjectPtr<UInteractiveToolBuilder> ToolBuilder)
	{
		// FUICommandInfo is uninitialized, it will be created later in FDataflowEditorCommandsImpl::RegisterCommands
		NodeTypeToToolMap.Add(NodeName, { ToolBuilder, nullptr });
	}

	void FDataflowToolRegistry::RemoveNodeToToolMapping(const FName& NodeName)
	{
		NodeTypeToToolMap.Remove(NodeName);
	}

	TArray<FName> FDataflowToolRegistry::GetNodeNames() const
	{
		TArray<FName> NodeNames;
		NodeTypeToToolMap.GetKeys(NodeNames);
		return NodeNames;
	}
	
	TSharedPtr<FUICommandInfo>& FDataflowToolRegistry::GetToolCommandForNode(const FName& NodeName)
	{
		check(NodeTypeToToolMap.Contains(NodeName));
		return NodeTypeToToolMap[NodeName].ToolCommand;
	}

	UInteractiveToolBuilder* FDataflowToolRegistry::GetToolBuilderForNode(const FName& NodeName)
	{
		check(NodeTypeToToolMap.Contains(NodeName));
		return NodeTypeToToolMap[NodeName].ToolBuilder;
	}

	const UInteractiveToolBuilder* FDataflowToolRegistry::GetToolBuilderForNode(const FName& NodeName) const
	{
		check(NodeTypeToToolMap.Contains(NodeName));
		return NodeTypeToToolMap[NodeName].ToolBuilder;
	}

}
