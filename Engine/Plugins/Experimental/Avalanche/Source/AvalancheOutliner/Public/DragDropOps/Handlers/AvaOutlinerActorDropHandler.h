// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerItemDropHandler.h"

/** Class that handles Dropping Actor Items into a Target Item */
class AVALANCHEOUTLINER_API FAvaOutlinerActorDropHandler : public FAvaOutlinerItemDropHandler
{
public:
	UE_AVA_INHERITS(FAvaOutlinerActorDropHandler, FAvaOutlinerItemDropHandler);

protected:
	//~ Begin FAvaOutlinerItemDropHandler
	virtual bool IsDraggedItemSupported(const FAvaOutlinerItemPtr& InDraggedItem) const override;
	virtual TOptional<EItemDropZone> CanDrop(EItemDropZone InDropZone, FAvaOutlinerItemPtr InTargetItem) const override;
	virtual bool Drop(EItemDropZone InDropZone, FAvaOutlinerItemPtr InTargetItem) override;
	//~ End FAvaOutlinerItemDropHandler

	void MoveItems(EItemDropZone InDropZone, FAvaOutlinerItemPtr InTargetItem);
};
