// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"

struct FDMXEntityFixturePatchRef;
struct FSlateColor;
template <typename OptionType> class SComboBox;
class SDMXControlConsoleEditorFaderGroupView;
class SSearchBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;
class UDMXLibrary;


/** Toolbar widget for the Fader Group view */
class SDMXControlConsoleEditorFaderGroupToolbar
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupToolbar)
	{}
		/** Executed when a Fader Group widget is added */
		SLATE_EVENT(FSimpleDelegate, OnAddFaderGroup)

		/** Executed when a new Fader Group Row widget is added */
		SLATE_EVENT(FSimpleDelegate, OnAddFaderGroupRow)
	
		/** Executed when the Fader Group View is expanded */
		SLATE_EVENT(FDMXControleConsolEditorExpandArrowButtonDelegate, OnExpanded)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& InFaderGroupView, UDMXControlConsoleEditorModel* InEditorModel);

	/** Generates the Fader Group settings menu widget content */
	TSharedRef<SWidget> GenerateSettingsMenuWidget();

	/** Gets a reference to this widget's ExpandArrow button */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> GetExpandArrowButton() const { return ExpandArrowButton; }

private:
	/** Gets reference to the Fader Group */
	UDMXControlConsoleFaderGroup* GetFaderGroup() const;

	/** Generates a widget for each element in the Fixture Patches Combo Box */
	TSharedRef<SWidget> GenerateFixturePatchesComboBoxWidget(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef);

	/** Generates a menu widget for the Fader Group info panel */
	TSharedRef<SWidget> GenerateFaderGroupInfoMenuWidget();

	/** Generates a menu widget for adding a new Fader Group to the Control Console */
	TSharedRef<SWidget> GenerateAddNewFaderGroupMenuWidget();

	/** Restores the search filter text from the Fader Group */
	void RestoreFaderGroupFilter();

	/** True if the given Fixture Patch is not used by any other Fader Group */
	bool IsFixturePatchStillAvailable(const UDMXEntityFixturePatch* InFixturePatch) const;

	/** Updates the ComboBoxSource array according to the current DMX Library */
	void UpdateComboBoxSource();

	/** Called when a FixturePatchesComboBox element is selected */
	void OnComboBoxSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef, ESelectInfo::Type SelectInfo);

	/** Called when the search text changed */
	void OnSearchTextChanged(const FText& SearchText);

	/** Adds a new Fader Group to the owner row */
	void OnAddFaderGroup() const;

	/** Adds a new Fader Group Row next to the owner row */
	void OnAddFaderGroupRow() const;

	/** True if a new Fader Group can be added next to this */
	bool CanAddFaderGroup() const;

	/** True if a new Fader Group can be added on the next row */
	bool CanAddFaderGroupRow() const;

	/** Called to generate the Fader Group Info Panel */
	void OnGetInfoPanel();

	/** Called to select all Faders in the Fader Group */
	void OnSelectAllFaders() const;

	/** Called when the duplicate option is selected */
	void OnDuplicateFaderGroup() const;

	/** Gets wheter the duplicate option is allowed or not */
	bool CanDuplicateFaderGroup() const;

	/** Called when the remove option is selected */
	void OnRemoveFaderGroup() const;

	/** Gets wheter the remove option is allowed or not */
	bool CanRemoveFaderGroup() const;

	/** Called when the reset option is selected */
	void OnResetFaderGroup() const;

	/** Called when the lock option is selected */
	void OnLockFaderGroup(bool bLock) const;

	/** Gets the fader group's editor color */
	FSlateColor GetFaderGroupEditorColor() const;

	/** Gets the fader group's fixture patch name, if valid */
	FText GetFaderGroupFixturePatchNameText() const;

	/** Gets visibility for the toolbar sections visible only in expanded view mode */
	EVisibility GetExpandedViewModeVisibility() const;

	/** Expander arrow button for showing/hiding the Faders widgets */
	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;

	/** Reference to the Fader Group toolbar searchbox used for filtering */
	TSharedPtr<SSearchBox> ToolbarSearchBox;

	/** Reference to the current DMX Library */
	TWeakObjectPtr<UDMXLibrary> DMXLibrary;

	/** Source items for the FixturePatchesComboBox */
	TArray<TSharedPtr<FDMXEntityFixturePatchRef>> ComboBoxSource;

	/** A ComboBox for showing all active Fixture Patches in the current DMX Library */
	TSharedPtr<SComboBox<TSharedPtr<FDMXEntityFixturePatchRef>>> FixturePatchesComboBox;

	/** Weak Reference to the Fader Group view */
	TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;

	// Slate Arguments
	FSimpleDelegate OnAddFaderGroupDelegate;
	FSimpleDelegate OnAddFaderGroupRowDelegate;
};
