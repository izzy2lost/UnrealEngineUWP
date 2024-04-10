// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SDMXControlConsoleEditorCueList.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FReply;
class SInlineEditableTextBlock;


namespace UE::DMX::Private
{
	DECLARE_DELEGATE_OneParam(FDMXControleConsolEditorCueListItemDelegate, TSharedPtr<FDMXControlConsoleEditorCueListItem>)
	DECLARE_DELEGATE_TwoParams(FDMXControleConsolEditorMoveCueListItemDelegate, TSharedPtr<FDMXControlConsoleEditorCueListItem>, EListItemMoveDirection)

	/** A Control Console Cue as a row in a list */
	class SDMXControlConsoleEditorCueListRow
		: public SMultiColumnTableRow<TSharedPtr<FDMXControlConsoleEditorCueListItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorCueListRow)
			{}

			/** Executed when the color of a cue list item is edited */
			SLATE_EVENT(FDMXControleConsolEditorCueListItemDelegate, OnEditCueItemColor)

			/** Executed when a cue list item is renamed */
			SLATE_EVENT(FDMXControleConsolEditorCueListItemDelegate, OnRenameCueItem)

			/** Executed when a cue list item is moved in a new position */
			SLATE_EVENT(FDMXControleConsolEditorMoveCueListItemDelegate, OnMoveCueItem)

			/** Executed when a cue list item is deleted */
			SLATE_EVENT(FDMXControleConsolEditorCueListItemDelegate, OnDeleteCueItem)

		SLATE_END_ARGS()

		/** Constructs this widget */
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXControlConsoleEditorCueListItem>& InItem);

	protected:
		//~ Begin SMultiColumnTableRow interface
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
		//~ End SMultiColumnTableRow interface

		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		//~ End of SWidget interface

	private:
		/** Generates the row that displays the color of the Cue */
		TSharedRef<SWidget> GenerateCueColorRow();

		/** Generates the row that displays the name label of the Cue */
		TSharedRef<SWidget> GenerateCueNameRow();

		/** Generates the row that displays the edit options for the Cue */
		TSharedRef<SWidget> GenerateCueOptionsRow();

		/** Called when the color section of this row is clicked */
		FReply OnCueColorMouseButtonClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

		/** Called when the color picker value is committed */
		void OnSetCueColorFromColorPicker(FLinearColor NewColor);

		/** Called when the text in the cue name text box is committed */
		void OnCueNameTextCommitted(const FText& NewName, ETextCommit::Type InCommit);

		/** Called when the move button is clicked */
		FReply OnMoveItemClicked(EListItemMoveDirection MoveDirection);

		/** Called when the delete button is clicked */
		FReply OnDeleteItemClicked();

		/** The editable text block that shows the name label of the cue item this row is based on */
		TSharedPtr<SInlineEditableTextBlock> CueLabelEditableTextBlock;

		/** The item this widget draws */
		TSharedPtr<FDMXControlConsoleEditorCueListItem> Item;

		// Slate Arguments
		FDMXControleConsolEditorCueListItemDelegate OnEditCueItemColorDelegate;
		FDMXControleConsolEditorCueListItemDelegate OnRenameCueItemDelegate;
		FDMXControleConsolEditorMoveCueListItemDelegate OnMoveCueItemDelegate;
		FDMXControleConsolEditorCueListItemDelegate OnDeleteCueItemDelegate;
	};
}
