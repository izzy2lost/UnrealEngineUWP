// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutputTreeItem.h"
#include "DragDropOps/AvaOutputTreeItemDragDropOp.h"

const TWeakPtr<FAvaOutputTreeItem>& FAvaOutputTreeItem::GetParent() const
{
	return ParentWeak;
}

const TArray<FAvaOutputTreeItemPtr>& FAvaOutputTreeItem::GetChildren() const
{
	return Children;
}

FReply FAvaOutputTreeItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().BeginDragDrop(FAvaOutputTreeItemDragDropOp::New(SharedThis(this)));
	}
	return FReply::Unhandled();
}

void FAvaOutputTreeItem::RefreshTree(const FAvaOutputTreeItemPtr& InItem)
{
	TArray<FAvaOutputTreeItemPtr> ItemsRemainingToRefresh;
	ItemsRemainingToRefresh.Add(InItem);
		
	while (ItemsRemainingToRefresh.Num() > 0)
	{
		FAvaOutputTreeItemPtr Item = ItemsRemainingToRefresh.Pop();
		if (Item.IsValid())
		{
			Item->RefreshChildren();
			ItemsRemainingToRefresh.Append(Item->GetChildren());
		}
	}
}
