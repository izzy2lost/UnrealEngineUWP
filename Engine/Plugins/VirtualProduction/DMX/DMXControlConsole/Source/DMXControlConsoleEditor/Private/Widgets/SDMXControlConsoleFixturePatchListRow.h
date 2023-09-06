// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SDMXControlConsoleFixturePatchList.h"
#include "Widgets/SDMXReadOnlyFixturePatchListRow.h"

class FDMXControlConsoleFixturePatchListRowModel;
class UDMXControlConsoleFaderGroup;


/** Entity Fixture Patch as a row in a list in DMX Control Console */
class SDMXControlConsoleFixturePatchListRow
	: public SDMXReadOnlyFixturePatchListRow
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleFixturePatchListRow)
	{}
		/** Delegate broadcast when the fader group muted state changed */
		SLATE_EVENT(FSimpleDelegate, OnFaderGroupMutedChanged)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXReadOnlyFixturePatchListItem>& InItem);

protected:
	//~ Begin SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End SMultiColumnTableRow interface

private:
	/** Generates the row that displays the check box for Fixture Patch active state */
	TSharedRef<SWidget> GenerateCheckBoxRow();

	/** Model for this row */
	TSharedPtr<FDMXControlConsoleFixturePatchListRowModel> RowModel;

	// Slate arguments
	FSimpleDelegate OnFaderGroupMutedChanged;
};
