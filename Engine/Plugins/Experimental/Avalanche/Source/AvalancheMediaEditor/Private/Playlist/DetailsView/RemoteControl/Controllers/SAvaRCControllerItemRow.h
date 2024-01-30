// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRCControllerItem.h"
#include "SAvaRCControllerPanel.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class SAvaRCControllerItemRow : public SMultiColumnTableRow<FAvaRCControllerItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SAvaRCControllerItemRow) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SAvaRCControllerPanel> InControllerPanel, 
		const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRCControllerItem>& InRowItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

protected:
	TWeakPtr<const FAvaRCControllerItem> ItemPtrWeak;
	TWeakPtr<SAvaRCControllerPanel> ControllerPanelWeak;
};
