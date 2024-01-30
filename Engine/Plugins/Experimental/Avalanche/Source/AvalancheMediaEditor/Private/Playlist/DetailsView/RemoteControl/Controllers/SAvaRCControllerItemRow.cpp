// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRCControllerItemRow.h"
#include "AvaRCControllerItem.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

void SAvaRCControllerItemRow::Construct(const FArguments& InArgs, TSharedRef<SAvaRCControllerPanel> InControllerPanel,
	const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRCControllerItem>& InRowItem)
{
	ItemPtrWeak = InRowItem;
	ControllerPanelWeak = InControllerPanel;

	SMultiColumnTableRow<FAvaRCControllerItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SAvaRCControllerItemRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	TSharedPtr<const FAvaRCControllerItem> ItemPtr = ItemPtrWeak.Pin();

	if (ItemPtr.IsValid())
	{
		if (InColumnName == SAvaRCControllerPanel::ControllerColumnName)
		{
			return SNew(SScissorRectBox)
				[
					SNew(SBox)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.Padding(3.f, 2.f, 3.f, 2.f)
					[
						SNew(STextBlock)
						.Text(ItemPtr->GetDisplayName())
					]
				];
		}
		else if (InColumnName == SAvaRCControllerPanel::ValueColumnName)
		{
			if (ItemPtr->GetNodeWidgets().ValueWidget.IsValid())
			{
				return ItemPtr->GetNodeWidgets().ValueWidget.ToSharedRef();
			}
			else if (ItemPtr->GetNodeWidgets().WholeRowWidget.IsValid())
			{
				return ItemPtr->GetNodeWidgets().WholeRowWidget.ToSharedRef();
			}
		}
		else
		{
			TSharedPtr<SAvaRCControllerPanel> ControllerPanel = ControllerPanelWeak.Pin();

			if (ControllerPanel.IsValid())
			{
				TSharedPtr<SWidget> Cell = nullptr;
				const TArray<FAvaRCControllerTableRowExtensionDelegate>& TableRowExtensionDelegates = ControllerPanel->GetTableRowExtensionDelegates(InColumnName);

				for (const FAvaRCControllerTableRowExtensionDelegate& TableRowExtensionDelegate : TableRowExtensionDelegates)
				{
					TableRowExtensionDelegate.ExecuteIfBound(ControllerPanel.ToSharedRef(), ItemPtr.ToSharedRef(), Cell);
				}

				if (Cell.IsValid())
				{
					return Cell.ToSharedRef();
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}
