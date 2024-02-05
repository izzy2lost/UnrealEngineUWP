// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAvaRundownPageRemoteControlProps.h"
#include "Widgets/Views/STableRow.h"

class SBox;
class SWidget;

class SAvaRundownRCPropertyItemRow : public SMultiColumnTableRow<FAvaRundownRCPropertyItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SAvaRundownRCPropertyItemRow) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<SAvaRundownPageRemoteControlProps> InPropertyPanel,
		const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRundownRCPropertyItem>& InRowItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override;

	void UpdateValue();

protected:
	TWeakPtr<const FAvaRundownRCPropertyItem> ItemPtrWeak;
	TWeakPtr<SAvaRundownPageRemoteControlProps> PropertyPanelWeak;
	TSharedPtr<IPropertyRowGenerator> Generator;
	TSharedPtr<SBox> ValueContainer;
	TSharedPtr<SWidget> ValueWidget;

	/** Get this field's label. */
	FText GetFieldLabel() const;

	TSharedRef<SWidget> CreateValue();
};
