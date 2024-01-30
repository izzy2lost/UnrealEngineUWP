// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailTreeNode.h"
#include "SAvaPageRemoteControlProps.h"
#include "Widgets/Views/STableRow.h"

class SBox;
class SWidget;

class SAvaRCPropertyItemRow : public SMultiColumnTableRow<FAvaRCPropertyItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SAvaRCPropertyItemRow) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SAvaPageRemoteControlProps> InPropertyPanel,
		const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRCPropertyItem>& InRowItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

	void UpdateValue();

protected:
	TWeakPtr<const FAvaRCPropertyItem> ItemPtrWeak;
	TWeakPtr<SAvaPageRemoteControlProps> PropertyPanelWeak;
	TSharedPtr<IPropertyRowGenerator> Generator;
	TSharedPtr<SBox> ValueContainer;
	TSharedPtr<SWidget> ValueWidget;

	/** Get this field's label. */
	FText GetFieldLabel() const;

	TSharedRef<SWidget> CreateValue();
};
