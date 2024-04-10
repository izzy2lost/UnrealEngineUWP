// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorCueList.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderBase.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Widgets/SDMXControlConsoleEditorCueListRow.h"
#include "Widgets/Views/SListView.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorCueList"

namespace UE::DMX::Private
{
	const FName FDMXControlConsoleEditorCueListColumnIDs::Color = "Color";
	const FName FDMXControlConsoleEditorCueListColumnIDs::Name = "Name";
	const FName FDMXControlConsoleEditorCueListColumnIDs::Options = "Options";

	FDMXControlConsoleEditorCueListItem::FDMXControlConsoleEditorCueListItem(const FDMXControlConsoleCue& InCue)
		: Cue(InCue)
	{}

	FText FDMXControlConsoleEditorCueListItem::GetCueNameText() const
	{
		return FText::FromString(Cue.CueLabel);
	}

	void FDMXControlConsoleEditorCueListItem::SetCueName(const FString& CueLabel)
	{
		Cue.CueLabel = CueLabel;
	}

	FSlateColor FDMXControlConsoleEditorCueListItem::GetCueColor() const
	{
		return Cue.CueColor;
	}

	void FDMXControlConsoleEditorCueListItem::SetCueColor(const FLinearColor CueColor)
	{
		Cue.CueColor = CueColor;
	}

	void SDMXControlConsoleEditorCueList::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct cue list correctly.")))
		{
			return;
		}

		WeakEditorModel = InEditorModel;

		ChildSlot
			[
				SAssignNew(CueListView, SListView<TSharedPtr<FDMXControlConsoleEditorCueListItem>>)
				.HeaderRow(GenerateHeaderRow())
				.ListItemsSource(&CueListItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SDMXControlConsoleEditorCueList::OnGenerateRow)
				.OnSelectionChanged(this, &SDMXControlConsoleEditorCueList::OnSelectionChanged)
			];

		UpdateCueListItems();
	}

	TArray<TSharedPtr<FDMXControlConsoleEditorCueListItem>> SDMXControlConsoleEditorCueList::GetSelectedCueItems() const
	{
		TArray<TSharedPtr<FDMXControlConsoleEditorCueListItem>> SelectedItems;
		if (CueListView.IsValid())
		{
			SelectedItems = CueListView->GetSelectedItems();
		}

		return SelectedItems;
	}

	void SDMXControlConsoleEditorCueList::RequestRefresh()
	{
		UpdateCueListItems();
	}

	void SDMXControlConsoleEditorCueList::UpdateCueListItems()
	{
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		const UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return;
		}

		CueListItems.Reset();

		const TArray<FDMXControlConsoleCue>& CuesArray = ControlConsoleCueStack->GetCuesArray();
		for (const FDMXControlConsoleCue& Cue : CuesArray)
		{
			const TSharedRef<FDMXControlConsoleEditorCueListItem> CueListItem = MakeShared<FDMXControlConsoleEditorCueListItem>(Cue);
			CueListItems.Add(CueListItem);
		}

		if (CueListView.IsValid())
		{
			CueListView->RebuildList();
		}
	}

	TSharedRef<SHeaderRow> SDMXControlConsoleEditorCueList::GenerateHeaderRow()
	{
		TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow);

		HeaderRow->AddColumn
		(
			SHeaderRow::FColumn::FArguments()
			.ColumnId(FDMXControlConsoleEditorCueListColumnIDs::Color)
			.DefaultLabel(LOCTEXT("EditorColorColumnLabel", ""))
			.FixedWidth(16.f)
		);

		HeaderRow->AddColumn
		(
			SHeaderRow::FColumn::FArguments()
			.ColumnId(FDMXControlConsoleEditorCueListColumnIDs::Name)
			.FillWidth(0.25f)
			.VAlignHeader(VAlign_Center)
			.HeaderContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NameColumnLabel", "Name"))
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		);

		HeaderRow->AddColumn
		(
			SHeaderRow::FColumn::FArguments()
			.ColumnId(FDMXControlConsoleEditorCueListColumnIDs::Options)
			.DefaultLabel(LOCTEXT("OptionsColumnLabel", ""))
			.FixedWidth(78.f)
		);

		return HeaderRow;
	}

	TSharedRef<ITableRow> SDMXControlConsoleEditorCueList::OnGenerateRow(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SDMXControlConsoleEditorCueListRow, OwnerTable, InItem.ToSharedRef())
			.OnEditCueItemColor(this, &SDMXControlConsoleEditorCueList::OnEditCueItemColor)
			.OnRenameCueItem(this, &SDMXControlConsoleEditorCueList::OnRenameCueItem)
			.OnMoveCueItem(this, &SDMXControlConsoleEditorCueList::OnMoveCueItem)
			.OnDeleteCueItem(this, &SDMXControlConsoleEditorCueList::OnDeleteCueItem);
	}

	void SDMXControlConsoleEditorCueList::OnSelectionChanged(const TSharedPtr<FDMXControlConsoleEditorCueListItem> NewSelection, ESelectInfo::Type SelectInfo)
	{
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack || !NewSelection.IsValid())
		{
			return;
		}

		const FScopedTransaction RecallCueTransaction(LOCTEXT("RecallCueTransaction", "Recall Cue"));

		// Synch controllers to the new fader values
		const FDMXControlConsoleCue& SelectedCue = NewSelection->GetCue();
		const TMap<TWeakObjectPtr<UDMXControlConsoleFaderBase>, uint32>& FaderToValueMap = SelectedCue.FaderToValueMap;
		for (const TTuple<TWeakObjectPtr<UDMXControlConsoleFaderBase>, uint32>& FaderToValue : FaderToValueMap)
		{
			const UDMXControlConsoleFaderBase* Fader = FaderToValue.Key.Get();
			if (!Fader)
			{
				continue;
			}

			UDMXControlConsoleElementController* ElementController = Cast<UDMXControlConsoleElementController>(Fader->GetElementController());
			if (!ElementController)
			{
				continue;
			}

			const uint32 Value = FaderToValue.Value;
			const bool bHasSingleElement = ElementController->GetElements().Num() == 1;
			if (bHasSingleElement)
			{
				const uint8 NumChannels = static_cast<uint8>(Fader->GetDataType()) + 1;
				const uint32 ValueRange = static_cast<uint32>(FMath::Pow(2.f, 8.f * NumChannels) - 1);
				const float NormalizedValue = static_cast<float>(Value) / ValueRange;

				ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetValuePropertyName()));
				constexpr bool bSynchElements = false;
				ElementController->SetValue(NormalizedValue, bSynchElements);
				ElementController->PostEditChange();
			}
		}

		// Recall the selected cue
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->Recall(SelectedCue);
		ControlConsoleCueStack->PostEditChange();
	}

	void SDMXControlConsoleEditorCueList::OnEditCueItemColor(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem)
	{
		if (!InItem.IsValid() || !WeakEditorModel.IsValid())
		{
			return;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return;
		}

		const FGuid CueID = InItem->GetCue().CueID;
		if (FDMXControlConsoleCue* Cue = ControlConsoleCueStack->FindCue(CueID))
		{
			const FScopedTransaction EditCueColorTransaction(LOCTEXT("EditCueColorTransaction", "Edit Cue color"));
			ControlConsoleCueStack->Modify();
			Cue->CueColor = InItem->GetCue().CueColor;

			UpdateCueListItems();
		}
	}

	void SDMXControlConsoleEditorCueList::OnRenameCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem)
	{
		if (!InItem.IsValid() || !WeakEditorModel.IsValid())
		{
			return;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return;
		}

		const FGuid CueID = InItem->GetCue().CueID;
		if (FDMXControlConsoleCue* Cue = ControlConsoleCueStack->FindCue(CueID))
		{
			const FScopedTransaction RenameCueTransaction(LOCTEXT("RenameCueTransaction", "Rename Cue"));
			ControlConsoleCueStack->Modify();
			Cue->CueLabel = InItem->GetCue().CueLabel;

			UpdateCueListItems();
		}
	}

	void SDMXControlConsoleEditorCueList::OnMoveCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem, EListItemMoveDirection MoveDirection)
	{
		if (!InItem.IsValid() || !WeakEditorModel.IsValid())
		{
			return;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return;
		}

		const FDMXControlConsoleCue& Cue = InItem->GetCue();
		const int32 CueIndex = ControlConsoleCueStack->GetCuesArray().IndexOfByKey(Cue);
		int32 NewCueIndex = CueIndex;
		if (MoveDirection == EListItemMoveDirection::Previous)
		{
			NewCueIndex--;
		}
		else if (MoveDirection == EListItemMoveDirection::Next)
		{
			NewCueIndex++;
		}
		else
		{
			return;
		}

		const FScopedTransaction MoveCueTransaction(LOCTEXT("MoveCueTransaction", "Move Cue"));
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->MoveCueToIndex(Cue, NewCueIndex);
		ControlConsoleCueStack->PostEditChange();

		UpdateCueListItems();
	}

	void SDMXControlConsoleEditorCueList::OnDeleteCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem)
	{
		if (!InItem.IsValid() || !WeakEditorModel.IsValid())
		{
			return;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack)
		{
			return;
		}

		const FDMXControlConsoleCue& CueToDelete = InItem->GetCue();

		const FScopedTransaction DeleteCueTransaction(LOCTEXT("DeleteCueTransaction", "Delete Cue"));
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->RemoveCue(CueToDelete);
		ControlConsoleCueStack->PostEditChange();

		UpdateCueListItems();
	}
}

#undef LOCTEXT_NAMESPACE
