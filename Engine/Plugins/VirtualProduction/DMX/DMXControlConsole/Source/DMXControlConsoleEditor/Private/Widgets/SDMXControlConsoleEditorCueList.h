// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleCueStack.h"
#include "Widgets/SCompoundWidget.h"

struct FDMXControlConsoleCue;
class FDMXControlConsoleEditorCueListItem;
class ITableRow;
class SHeaderRow;
template <typename ItemType> class SListView;
class STableViewBase;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** Collumn ids in the cue list */
	struct FDMXControlConsoleEditorCueListColumnIDs
	{
		static const FName Color;
		static const FName Name;
		static const FName Options;
	};

	/** Enum class to specify the move direction of a cue list item */
	enum class EListItemMoveDirection
	{
		Previous,
		Next
	};

	/** An item in the Control Console cue list */
	class FDMXControlConsoleEditorCueListItem
		: public TSharedFromThis<FDMXControlConsoleEditorCueListItem>
	{
	public:
		/** Constructor */
		FDMXControlConsoleEditorCueListItem(const FDMXControlConsoleCue& InCue);

		/** Returns the cue this item uses */
		FDMXControlConsoleCue GetCue() const { return Cue; }

		/** Returns the name label of the cue this item uses, as text */
		FText GetCueNameText() const;

		/** Sets the name label of the cue this item uses */
		void SetCueName(const FString& CueLabel);

		/** Returns the color of the cue this item uses */
		FSlateColor GetCueColor() const;

		/** Sets the color of the cue this item uses */
		void SetCueColor(const FLinearColor CueColor);

	private:
		/** The cue this item is based on */
		FDMXControlConsoleCue Cue;
	};

	/** List of Cues in a DMX Control Console */
	class SDMXControlConsoleEditorCueList
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorCueList)
			{}

		SLATE_END_ARGS()

		/** Constructs this widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the array of current selected cue items */
		TArray<TSharedPtr<FDMXControlConsoleEditorCueListItem>> GetSelectedCueItems() const;

		/** Requests the refresh of the list */
		void RequestRefresh();

	private:
		/** Updates the array of Cue List Items */
		void UpdateCueListItems();

		/** Called to generate the header row of the list */
		TSharedRef<SHeaderRow> GenerateHeaderRow();

		/** Called to generate a row in the list */
		TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

		/** Called when selection in the list changed */
		void OnSelectionChanged(const TSharedPtr<FDMXControlConsoleEditorCueListItem> NewSelection, ESelectInfo::Type SelectInfo);

		/** Called when the color of an item in the list is changed */
		void OnEditCueItemColor(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem);

		/** Called when the name label of an item in the list is changed */
		void OnRenameCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem);

		/** Called when an item in the list is deleted */
		void OnMoveCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem, EListItemMoveDirection MoveDirection);

		/** Called when an item in the list is deleted */
		void OnDeleteCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem);

		/** Reference to the Cue List View widget */
		TSharedPtr<SListView<TSharedPtr<FDMXControlConsoleEditorCueListItem>>> CueListView;

		/** The array of Cue List Items this list is based on */
		TArray<TSharedPtr<FDMXControlConsoleEditorCueListItem>> CueListItems;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
