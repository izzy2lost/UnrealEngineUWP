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

	SelectedItems.Add(Pin);
	SelectPin(Pin, true);

	OnPinSelectionUpdated.Broadcast(Pin);
}

void FTG_PinSelectionManager::ClearPinsForNonSelectedNodes(TArray<UObject*> SelectedNodes)
{
	for (int32 i = SelectedItems.Num() - 1; i >= 0; --i)
	{
		if (!SelectedNodes.Contains(Cast<UTG_EdGraphNode>(SelectedItems[i]->GetOwningNode())->GetDetailsObject()))
		{
			SelectedItems.RemoveAt(i);
		}
	}
}

void FTG_PinSelectionManager::SelectPin(UEdGraphPin* Pin,bool IsSelected)
{
	auto Node = Cast<UTG_EdGraphNode>(Pin->GetOwningNode());
	check(Node);
	Node->SelectPin(Pin, IsSelected);
}