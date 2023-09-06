// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
struct FDMXEntityFixturePatchRef;
class FReply;
class FUICommandList;
class SDMXControlConsoleFixturePatchList;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


/** A container for FixturePatchRow widgets */
class SDMXControlConsoleEditorFixturePatchVerticalBox
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFixturePatchVerticalBox)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Refreshes the widget */
	void ForceRefresh();

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
	//~ End SWidget interface

private:
	/** Generates a toolbar for FixturePatchList widget */
	TSharedRef<SWidget> GenerateFixturePatchListToolbar();

	/** Creates a menu for the Add Patch combo button */
	TSharedRef<SWidget> CreateAddPatchMenu();

	/** Edits the given Fader Group according to the given Fixture Patch */
	void GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch);

	/** Called on Add All Patches button click to generate Fader Groups form a Library */
	FReply OnAddAllPatchesClicked();

	/** Gets enable state for Add All Patches button when a DMX Library is selected */
	bool IsAddAllPatchesButtonEnabled() const;

	/** Gets visibility for FixturePatchList toolbar  */
	EVisibility GetFixturePatchListToolbarVisibility() const;

	/** Reference to FixturePatchList widget */
	TSharedPtr<SDMXControlConsoleFixturePatchList> FixturePatchList;

	/** Command list for this widget */
	TSharedPtr<FUICommandList> CommandList;
};
