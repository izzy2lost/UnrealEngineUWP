// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorCueList.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
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
	const FName FDMXControlConsoleEditorCueListColumnIDs::State = "State";
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

	TSharedRef<FDMXControlConsoleEditorCueListDragDropOp> FDMXControlConsoleEditorCueListDragDropOp::New(TWeakPtr<FDMXControlConsoleEditorCueListItem> InItem)
	{
		TSharedRef<FDMXControlConsoleEditorCueListDragDropOp> Operation = MakeShared<FDMXControlConsoleEditorCueListDragDropOp>();
		Operation->CueItem = InItem;
		Operation->Construct();
		return Operation;
	}

	TSharedPtr<SWidget> FDMXControlConsoleEditorCueListDragDropOp::GetDefaultDecorator() const
	{
		const TSharedPtr<FDMXControlConsoleEditorCueListItem> CueItemPtr = CueItem.Pin();
		if (!CueItemPtr.IsValid())
		{
			return FDecoratedDragDropOp::GetDefaultDecorator();
		}

		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			[
				SNew(STextBlock)
				.Text(CueItemPtr->GetCueNameText())
			];
	}

	void SDMXControlConsoleEditorCueList::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct cue list correctly.")))
		{
			return;
		}

		WeakEditorModel = InEditorModel;

		const UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel->GetControlConsoleEditorData();
		if (ControlConsoleEditorData)
		{
			LastSelectedCue = ControlConsoleEditorData->LoadedCue;
		}

		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel->GetControlConsoleData();
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (ControlConsoleCueStack)
		{
			ControlConsoleCueStack->GetOnCueStackChanged().AddSP(this, &SDMXControlConsoleEditorCueList::UpdateCueListItems);
		}

		ChildSlot
			[
				SAssignNew(CueListView, SListView<TSharedPtr<FDMXControlConsoleEditorCueListItem>>)
				.HeaderRow(GenerateHeaderRow())
				.ListItemsSource(&CueListItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SDMXControlConsoleEditorCueList::OnGenerateRow)
				.OnSelectionChanged(this, &SDMXControlConsoleEditorCueList::OnSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SDMXControlConsoleEditorCueList::OnRowDoubleClicked)
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
		if (!ControlConsoleCueStack || !CueListView.IsValid())
		{
			return;
		}

		CueListItems.Reset();

		TSharedPtr< FDMXControlConsoleEditorCueListItem> LastLoadedCueListItem;
		const TArray<FDMXControlConsoleCue>& CuesArray = ControlConsoleCueStack->GetCuesArray();
		for (const FDMXControlConsoleCue& Cue : CuesArray)
		{
			const TSharedRef<FDMXControlConsoleEditorCueListItem> CueListItem = MakeShared<FDMXControlConsoleEditorCueListItem>(Cue);
			if (Cue == LastSelectedCue)
			{
				LastLoadedCueListItem = CueListItem;
			}

			CueListItems.Add(CueListItem);
		}

		CueListView->RebuildList();
		if (LastLoadedCueListItem.IsValid())
		{
			constexpr bool bSelectItem = true;
			CueListView->SetItemSelection(LastLoadedCueListItem, bSelectItem);
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
			.ColumnId(FDMXControlConsoleEditorCueListColumnIDs::State)
			.DefaultLabel(LOCTEXT("StateColumnLabel", ""))
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
			.FixedWidth(102.f)
		);

		return HeaderRow;
	}

	TSharedRef<ITableRow> SDMXControlConsoleEditorCueList::OnGenerateRow(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SDMXControlConsoleEditorCueListRow, OwnerTable, InItem.ToSharedRef(), WeakEditorModel.Get())
			.OnEditCueItemColor(this, &SDMXControlConsoleEditorCueList::OnEditCueItemColor)
			.OnRenameCueItem(this, &SDMXControlConsoleEditorCueList::OnRenameCueItem)
			.OnMoveCueItem(this, &SDMXControlConsoleEditorCueList::OnMoveCueItem)
			.OnDeleteCueItem(this, &SDMXControlConsoleEditorCueList::OnDeleteCueItem)
			.OnDragDetected(this, &SDMXControlConsoleEditorCueList::OnRowDragDetected)
			.OnCanAcceptDrop(this, &SDMXControlConsoleEditorCueList::OnRowCanAcceptDrop)
			.OnAcceptDrop(this, &SDMXControlConsoleEditorCueList::OnRowAcceptDrop);
	}

	void SDMXControlConsoleEditorCueList::OnSelectionChanged(const TSharedPtr<FDMXControlConsoleEditorCueListItem> NewSelection, ESelectInfo::Type SelectInfo)
	{
		if (NewSelection.IsValid())
		{
			LastSelectedCue = NewSelection->GetCue();
		}

		if (SelectInfo != ESelectInfo::OnNavigation || !NewSelection.IsValid())
		{
			return;
		}

		UDMXControlConsoleEditorData* ControlConsoleEditorData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleEditorData() : nullptr;
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleEditorData || !ControlConsoleCueStack)
		{
			return;
		}

		const FScopedTransaction RecallCueTransaction(LOCTEXT("RecallCueTransaction", "Recall Cue"));
		const FDMXControlConsoleCue& SelectedCue = NewSelection->GetCue();

		// Update the loaded cue
		ControlConsoleEditorData->PreEditChange(nullptr);
		ControlConsoleEditorData->LoadedCue = SelectedCue;
		ControlConsoleEditorData->PostEditChange();

		// Synch controllers to the new fader values
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

	void SDMXControlConsoleEditorCueList::OnRowDoubleClicked(const TSharedPtr<FDMXControlConsoleEditorCueListItem> ItemClicked)
	{
		if (ItemClicked.IsValid())
		{
			OnSelectionChanged(ItemClicked, ESelectInfo::OnNavigation);
		}
	}

	FReply SDMXControlConsoleEditorCueList::OnRowDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (!CueListView.IsValid())
		{
			return FReply::Unhandled();
		}

		const TArray<TSharedPtr<FDMXControlConsoleEditorCueListItem>> SelectedCueItems = CueListView->GetSelectedItems();
		if (SelectedCueItems.Num() != 1)
		{
			return FReply::Unhandled();
		}

		if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			const TSharedPtr<FDMXControlConsoleEditorCueListItem> DraggedItem = SelectedCueItems[0];
			const TSharedRef<FDMXControlConsoleEditorCueListDragDropOp> DragDropOp = FDMXControlConsoleEditorCueListDragDropOp::New(DraggedItem);
			return FReply::Handled().BeginDragDrop(DragDropOp);
		}

		return FReply::Unhandled();
	}

	TOptional<EItemDropZone> SDMXControlConsoleEditorCueList::OnRowCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FDMXControlConsoleEditorCueListItem> TargetItem)
	{
		TOptional<EItemDropZone> ItemDropZone;

		const TSharedPtr<FDMXControlConsoleEditorCueListDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDMXControlConsoleEditorCueListDragDropOp>();
		if (DragDropOp.IsValid())
		{
			ItemDropZone = DropZone == EItemDropZone::BelowItem ? DropZone : EItemDropZone::AboveItem;
		}

		return ItemDropZone;
	}

	FReply SDMXControlConsoleEditorCueList::OnRowAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FDMXControlConsoleEditorCueListItem> TargetItem)
	{
		const UDMXControlConsoleData* ControlConsoleData = WeakEditorModel.IsValid() ? WeakEditorModel->GetControlConsoleData() : nullptr;
		UDMXControlConsoleCueStack* ControlConsoleCueStack = ControlConsoleData ? ControlConsoleData->GetCueStack() : nullptr;
		if (!ControlConsoleCueStack || !TargetItem.IsValid() || DropZone == EItemDropZone::OntoItem)
		{
			return FReply::Unhandled();
		}

		const TSharedPtr<FDMXControlConsoleEditorCueListDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDMXControlConsoleEditorCueListDragDropOp>();
		if (!DragDropOp.IsValid())
		{
			return FReply::Unhandled();
		}

		const TSharedPtr<FDMXControlConsoleEditorCueListItem> DraggedItem = DragDropOp->CueItem.Pin();
		if (!DraggedItem.IsValid())
		{
			return FReply::Unhandled();
		}

		const FDMXControlConsoleCue& DraggedCue = DraggedItem->GetCue();
		const FDMXControlConsoleCue& TargetCue = TargetItem->GetCue();

		const int32 DraggedCueIndex = ControlConsoleCueStack->GetCuesArray().IndexOfByKey(DraggedCue);
		const int32 TargetCueIndex = ControlConsoleCueStack->GetCuesArray().IndexOfByKey(TargetCue);
		int32 NewDraggedCueIndex = TargetCueIndex;
		if (DraggedCueIndex > TargetCueIndex && DropZone == EItemDropZone::BelowItem)
		{
			NewDraggedCueIndex++;
		}
		else if (DraggedCueIndex < TargetCueIndex && DropZone == EItemDropZone::AboveItem)
		{
			NewDraggedCueIndex--;
		}

		const FScopedTransaction AcceptCueDropTransaction(LOCTEXT("AcceptCueDropTransaction", "Move Cue"));
		ControlConsoleCueStack->PreEditChange(nullptr);
		ControlConsoleCueStack->MoveCueToIndex(DraggedCue, NewDraggedCueIndex);
		ControlConsoleCueStack->PostEditChange();

		return FReply::Handled();
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

	void SDMXControlConsoleEditorCueList::OnMoveCueItem(TSharedPtr<FDMXControlConsoleEditorCueListItem> InItem, EItemDropZone DropZone)
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
		if (DropZone == EItemDropZone::AboveItem)
		{
			NewCueIndex--;
		}
		else if (DropZone == EItemDropZone::BelowItem)
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
	}
}

#undef LOCTEXT_NAMESPACE
