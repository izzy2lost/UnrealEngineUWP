// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SDMXReadOnlyFixturePatchList.h"

enum class ECheckBoxState : uint8;
class FDMXReadOnlyFixturePatchListItem;
class FUICommandList;
class UDMXControlConsoleEditorGlobalLayoutRow;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;
class UToolMenu;


/** Collumn IDs in the Fixture Patch List */
struct FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs
{
	static const FName FaderGroupEnabled;
};

/** List of Fixture Patches in a DMX library for read only purposes in DMX Control Console */
class SDMXControlConsoleFixturePatchList
	: public SDMXReadOnlyFixturePatchList
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleFixturePatchList)
		: _DMXLibrary(nullptr)
	{}

		/** The DMX Library displayed in the list */
		SLATE_ARGUMENT(UDMXLibrary*, DMXLibrary)

	SLATE_END_ARGS()

	/** Destructor */
	~SDMXControlConsoleFixturePatchList();

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

protected:
	//~ Begin SDMXReadOnlyFixturePatchList interface
	virtual FName GetHeaderRowFilterMenuName() const override;
	virtual void ForceRefresh() override;
	virtual TSharedRef<SHeaderRow> GenerateHeaderRow() override;
	virtual TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDMXReadOnlyFixturePatchListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable) override;
	virtual void ToggleColumnShowState(const FName ColumnID) override;
	//~ End of SDMXReadOnlyFixturePatchList interface

private:
	/** Extends the header row filter menu */
	void ExtendHeaderRowFilterMenu(UToolMenu* InMenu);

	/** Registers commands for this widget */
	void RegisterCommands();

	/** Adopts selection from data */
	void AdoptSelectionFromData();

	/** Called when a fader group was added or removed from the console */
	void OnFaderGroupAddedOrRemoved(const UDMXControlConsoleFaderGroup* FaderGroup);

	/** Called when a global layout row changed */
	void OnGlobalLayoutRowChanged(UDMXControlConsoleEditorGlobalLayoutRow* ChangedRow);

	/** Called when the context menu is opening */
	TSharedPtr<SWidget> OnContextMenuOpening();

	/** Called when selection in the list changed */
	void OnSelectionChanged(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> NewSelection, ESelectInfo::Type SelectInfo);

	/** Called when a row was clicked */
	void OnRowClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ItemClicked);

	/** Called when a row was double clicked */
	void OnRowDoubleClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ItemClicked);

	/** Called to mute/unmute all Fader Groups in current Control Console */
	void OnMuteAllFaderGroups(bool bMute, bool bOnlyActive) const;

	/** Gets wheter any Fader Group is muted/unmuted */
	bool IsAnyFaderGroupMuted(bool bMute, bool bOnlyActive) const;

	/** Called to get wheter the whole list is checked or not */
	ECheckBoxState GetGlobalFixtureGroupsMutedCheckBoxState() const;

	/** Called when the global fixture groups muted checkbox state changed */
	void OnGlobalFixtureGroupsMutedCheckBoxStateChanged(ECheckBoxState CheckBoxState);

	/** Sets the show mode to be used */
	void SetShowMode(EDMXReadOnlyFixturePatchListShowMode NewShowMode);

	/** Returns true if the current show mode matches the specified one */
	bool IsUsingShowMode(EDMXReadOnlyFixturePatchListShowMode InShowMode) const;

	/** Specifies if active, inactive or all patches are shown in the list */
	EDMXReadOnlyFixturePatchListShowMode ShowMode;

	/** Command list for this widget */
	TSharedPtr<FUICommandList> CommandList;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;

	/** Editor Model's unique identifier */
	int32 EditorModelUniqueID;
};
