// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FReply;
class SDMXControlConsoleFixturePatchList;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


/** A container for the Fixture Patch List widget */
class SDMXControlConsoleEditorFixturePatchVerticalBox
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFixturePatchVerticalBox)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

	/** Refreshes the widget */
	void ForceRefresh();

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
	//~ End SWidget interface

private:
	/** Generates a toolbar for the FixturePatchList widget */
	TSharedRef<SWidget> GenerateFixturePatchListToolbar();

	/** Creates a menu for the Add Patch combo button */
	TSharedRef<SWidget> CreateAddPatchMenu();

	/** Edits the given Fader Group according to the given Fixture Patch */
	void GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch);

	/** Called on Add All Patches button clicked to generate Fader Groups form a Library */
	FReply OnAddAllPatchesClicked();

	/** Gets the enable state for the Add All Patches button when a DMX Library is selected */
	bool IsAddAllPatchesButtonEnabled() const;

	/** Gets the visibility for the FixturePatchList toolbar  */
	EVisibility GetFixturePatchListToolbarVisibility() const;

	/** Reference to the FixturePatchList widget */
	TSharedPtr<SDMXControlConsoleFixturePatchList> FixturePatchList;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
};
