// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Broadcast/ChannelGrid/AvaOutputTileItem.h"
#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"

class FAvaOutputTileItemDragDropOp : public FDecoratedDragDropOp
{
public:

	DRAG_DROP_OPERATOR_TYPE(FAvaOutputTileDragDropOp, FDecoratedDragDropOp)
	
	static TSharedRef<FAvaOutputTileItemDragDropOp> New(const FAvaOutputTileItemPtr& InOutputClassItem, bool bInIsDuplicating);

	bool IsValidToDropInChannel(FName InTargetChannelName) const;
	
	FAvaOutputTileItemPtr GetOutputTileItem() const { return OutputTileItem; }

	FReply OnChannelDrop(FName InTargetChannelName);
	
protected:
	
	void Init(const FAvaOutputTileItemPtr& InOutputTileItem, bool bInIsDuplicating);

	//Keep Reference Count while Drag Dropping
	FAvaOutputTileItemPtr OutputTileItem;

	bool bIsDuplicating = false;
};
