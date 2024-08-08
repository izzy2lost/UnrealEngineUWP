// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"

class FUICommandInfo;
class UInteractiveToolBuilder;

namespace Dataflow
{
	class DATAFLOWEDITOR_API FDataflowToolRegistry
	{
	public:

		static FDataflowToolRegistry& Get();
		static void TearDown();

		void AddNodeToToolMapping(const FName& NodeName, TObjectPtr<UInteractiveToolBuilder> ToolBuilder);
		void RemoveNodeToToolMapping(const FName& NodeName);

		TArray<FName> GetNodeNames() const;

		TSharedPtr<FUICommandInfo>& GetToolCommandForNode(const FName& NodeName);

		UInteractiveToolBuilder* GetToolBuilderForNode(const FName& NodeName);
		const UInteractiveToolBuilder* GetToolBuilderForNode(const FName& NodeName) const;

	private:

		struct FToolInfo
		{
			// Specified when registering the tool
			TObjectPtr<UInteractiveToolBuilder> ToolBuilder;

			// Constructed automatically in FDataflowEditorCommandsImpl::RegisterCommands
			TSharedPtr<FUICommandInfo> ToolCommand;
		};

		TMap<FName, FToolInfo> NodeTypeToToolMap;
	
	};

}
