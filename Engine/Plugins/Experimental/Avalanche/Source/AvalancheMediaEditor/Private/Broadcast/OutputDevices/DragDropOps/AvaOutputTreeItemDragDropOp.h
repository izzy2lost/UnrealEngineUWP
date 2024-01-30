// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DragAndDrop/DecoratedDragDropOp.h"

class FAvaOutputTreeItem;

class FAvaOutputTreeItemDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAvaOutputClassDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FAvaOutputTreeItemDragDropOp> New(const TSharedPtr<FAvaOutputTreeItem>& InOutputClassItem);

	bool IsValidToDropInChannel(FName InTargetChannelName) const;

	TSharedPtr<FAvaOutputTreeItem> GetOutputTreeItem() const { return OutputTreeItem; }

	FReply OnChannelDrop(FName InTargetChannelName);

protected:
	void Init(const TSharedPtr<FAvaOutputTreeItem>& InOutputClassItem);

	/** Keep Reference Count while Drag Dropping */
	TSharedPtr<FAvaOutputTreeItem> OutputTreeItem;
};
