// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowBlueprintLibrary.h"

#include "Dataflow/DataflowObjectInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowBlueprintLibrary)

void UDataflowBlueprintLibrary::EvaluateTerminalNodeByName(UDataflow* Dataflow, FName TerminalNodeName, UObject* ResultAsset)
{
	if (Dataflow && Dataflow->Dataflow)
	{
		if (const TSharedPtr<FDataflowNode> Node = Dataflow->Dataflow->FindFilteredNode(FDataflowTerminalNode::StaticType(), TerminalNodeName))
		{
			if (const FDataflowTerminalNode* TerminalNode = Node->AsType<const FDataflowTerminalNode>())
			{
				UE_LOG(LogChaosDataflow, Verbose, TEXT("UDataflowBlueprintLibrary::EvaluateTerminalNodeByName(): Node [%s]"), *TerminalNodeName.ToString());
				Dataflow::FEngineContext Context(ResultAsset);
				TerminalNode->Evaluate(Context);
				if (ResultAsset)
				{
					UE_LOG(LogChaosDataflow, Verbose, TEXT("FDataflowTerminalNode::SetAssetValue(): TerminalNode [%s], Asset [%s]"), *TerminalNodeName.ToString(), *ResultAsset->GetName());
					TerminalNode->SetAssetValue(ResultAsset, Context);
				}
			}
		}
		else
		{
			UE_LOG(LogChaos, Warning, TEXT("EvaluateTerminalNodeByName : Could not find terminal node : [%s], skipping evaluation"), *TerminalNodeName.ToString());
		}
	}
}













