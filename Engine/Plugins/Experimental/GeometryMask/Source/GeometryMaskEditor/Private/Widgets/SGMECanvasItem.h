// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGMEImageItem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FGMECanvasItemViewModel;
class IGMETreeNodeViewModel;
class ITableRow;
class STableViewBase;

class SGMECanvasItem
	: public SGMEImageItem<FGMECanvasItemViewModel>
{
public:
	SLATE_BEGIN_ARGS(SGMECanvasItem) {}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedRef<FGMECanvasItemViewModel>& InViewModel);

protected:
	virtual FOptionalSize GetAspectRatio() override;

private:
	TSharedPtr<FGMECanvasItemViewModel> ViewModel;
};
