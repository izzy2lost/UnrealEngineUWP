// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/TG_PinSelectionManager.h"
#include "EdGraph/TG_EdGraphNode.h"

void FTG_PinSelectionManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	//Collector.AddReferencedObjects(SelectedItems);
}

void FTG_PinSelectionManager::UpdateSelection(UEdGraphPin* Pin)
{
	SelectedItems.Empty();

	if (Pin)
	{
		SelectedItems.Add(Pin);
		{
			auto Node = Cast<UTG_EdGraphNode>(Pin->GetOwningNode());
			check(Node);
			Node->SelectPin(Pin, true);
		}
	}

	OnPinSelectionUpdated.Broadcast(Pin);
}

void FTG_PinSelectionManager::ClearPinsForNonSelectedNodes(TArray<UObject*> SelectedNodes)
{
	// No Selected nodes means clear all
	if (SelectedNodes.IsEmpty())
	{
		SelectedItems.Empty();
	}
	else
	{
		for (int32 i = SelectedItems.Num() - 1; i >= 0; --i)
		{
			UEdGraphPin* SelectedItem = SelectedItems[i];
			if (SelectedItem)
			{
				UTG_EdGraphNode* SelectedPinNode = Cast<UTG_EdGraphNode>(SelectedItem->GetOwningNode());
				if (SelectedPinNode)
				{
					if (!SelectedNodes.Contains(Cast<UTG_EdGraphNode>(SelectedItems[i]->GetOwningNode())->GetDetailsObject()))
					{
						SelectedItems.RemoveAt(i);
					}
				}
			}
		}
	}
}
