// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleAddFixturePatchMenu.h"

#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleAddFixturePatchMenu"

void SDMXControlConsoleAddFixturePatchMenu::Construct(const FArguments& InArgs, TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> InFixturePatches, UDMXControlConsoleEditorModel* InEditorModel)
{
	EditorModel = InEditorModel;
	FixturePatches = InFixturePatches;

	RegisterCommands();
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("AddPatchButtonMainSection", "Add Patch"));
	{
		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchNext,
			NAME_None,
			LOCTEXT("AddPatchNextButtonLabel", "To the right")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchNextRow,
			NAME_None,
			LOCTEXT("AddPatchNextRowButtonLabel", "To next row")
		);

		MenuBuilder.AddMenuEntry
		(
			FDMXControlConsoleEditorCommands::Get().AddPatchToSelection,
			NAME_None,
			LOCTEXT("AddPatchToSelectionButtonLabel", "To selection")
		);
	}
	MenuBuilder.EndSection();

	ChildSlot
		[
			MenuBuilder.MakeWidget()
		];
}

void SDMXControlConsoleAddFixturePatchMenu::SetFixturePatches(TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> InFixturePatches)
{
	FixturePatches = InFixturePatches;
}

void SDMXControlConsoleAddFixturePatchMenu::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchNext,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::AddPatchesToTheRight),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesToTheRight)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchNextRow,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::AddPatchesOnNewRow),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesOnNewRow)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().AddPatchToSelection,
		FExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::SetPatchOnFaderGroup),
		FCanExecuteAction::CreateSP(this, &SDMXControlConsoleAddFixturePatchMenu::CanSetPatchOnFaderGroup)
	);
}

bool SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesToTheRight() const
{
	bool bCanExecute = !FixturePatches.IsEmpty();

	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (ControlConsoleData && ControlConsoleLayouts)
	{
		// True if there's no global filter and no vertical sorting
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
		bCanExecute &=
			IsValid(CurrentLayout) &&
			CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Vertical &&
			!CurrentLayout->GetAllFaderGroups().IsEmpty() &&
			ControlConsoleData->FilterString.IsEmpty();
	}

	return bCanExecute;
}

void SDMXControlConsoleAddFixturePatchMenu::AddPatchesToTheRight()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	int32 RowIndex = ActiveLayout->GetLayoutRows().Num() - 1;
	int32 ColumnIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (!SelectedFaderGroupsObjects.IsEmpty())
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup(true);
		RowIndex = ActiveLayout->GetFaderGroupRowIndex(SelectedFaderGroup);
		ColumnIndex = ActiveLayout->GetFaderGroupColumnIndex(SelectedFaderGroup) + 1;
	}

	// Add all selected Fixture Patches from Fixture Patch List
	for (const TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
	{
		UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		if (!FaderGroup)
		{
			continue;
		}

		if (ActiveLayout->GetAllFaderGroups().Contains(FaderGroup))
		{
			continue;
		}

		const FScopedTransaction AddToLastRowTransaction(LOCTEXT("AddToLastRowTransaction", "Add Fader Group"));
		ActiveLayout->PreEditChange(nullptr);
		ActiveLayout->AddToActiveFaderGroups(FaderGroup);
		if (ColumnIndex == INDEX_NONE)
		{
			ActiveLayout->AddToLayout(FaderGroup, RowIndex);
		}
		else
		{
			ActiveLayout->AddToLayout(FaderGroup, RowIndex, ColumnIndex);
			ColumnIndex++;
		}

		FaderGroup->Modify();
		FaderGroup->SetIsActive(true);

		ActiveLayout->PostEditChange();
	}
}

bool SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesOnNewRow() const
{
	bool bCanExecute = !FixturePatches.IsEmpty();

	const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
	if (ControlConsoleData && ControlConsoleLayouts)
	{
		// True if there's no global filter and no horizontal sorting
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
		bCanExecute &=
			IsValid(CurrentLayout) &&
			CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Horizontal &&
			ControlConsoleData->FilterString.IsEmpty();
	}

	return bCanExecute;
}

void SDMXControlConsoleAddFixturePatchMenu::AddPatchesOnNewRow()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	// Generate on last row if vertical sorting
	if (ActiveLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		AddPatchesToTheRight();
		return;
	}

	int32 NewRowIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		NewRowIndex = ActiveLayout->GetLayoutRows().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup(true);
		if (!SelectedFaderGroup)
		{
			return;
		}

		NewRowIndex = ActiveLayout->GetFaderGroupRowIndex(SelectedFaderGroup) + 1;
	}

	const FScopedTransaction AddToNewtRowTransaction(LOCTEXT("AddToNewtRowTransaction", "Add Fader Group"));
	ActiveLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(NewRowIndex);
	if (NewLayoutRow)
	{
		// Add all selected Fixture Patches from Fixture Patch List
		NewLayoutRow->PreEditChange(nullptr);
		for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
		{
			UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
			if (!FixturePatch)
			{
				continue;
			}

			UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			if (!FaderGroup)
			{
				continue;
			}

			if (ActiveLayout->GetAllFaderGroups().Contains(FaderGroup))
			{
				continue;
			}

			NewLayoutRow->AddToLayoutRow(FaderGroup);
			ActiveLayout->AddToActiveFaderGroups(FaderGroup);

			FaderGroup->Modify();
			FaderGroup->SetIsActive(true);
		}

		NewLayoutRow->PostEditChange();
	}

	ActiveLayout->PostEditChange();
}

bool SDMXControlConsoleAddFixturePatchMenu::CanSetPatchOnFaderGroup() const
{
	if (!EditorModel.IsValid() || FixturePatches.IsEmpty())
	{
		return false;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	if (!ControlConsoleData)
	{
		return false;
	}
		
	// True if there's if there's no global filter and at least one selected Fader Group
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	return ControlConsoleData->FilterString.IsEmpty() && !SelectionHandler->GetSelectedFaderGroups().IsEmpty();
}

void SDMXControlConsoleAddFixturePatchMenu::SetPatchOnFaderGroup()
{
	if (!EditorModel.IsValid())
	{
		return;
	}

	const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
	const UDMXControlConsoleFaderGroup* FirstSelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup();
	if (!FirstSelectedFaderGroup)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	const int32 RowIndex = ActiveLayout->GetFaderGroupRowIndex(FirstSelectedFaderGroup);

	const FScopedTransaction ReplaceSelectedFaderGroupTransaction(LOCTEXT("ReplaceSelectedFaderGroupTransaction", "Replace Fader Group"));
	ActiveLayout->PreEditChange(nullptr);

	// Add all Selected Patches Fader Groups to layout
	TArray<UObject*> FaderGroupsToSelect;

	int32 ColumnIndex = ActiveLayout->GetFaderGroupColumnIndex(FirstSelectedFaderGroup);
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
	{
		const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroupToAdd = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		ActiveLayout->AddToLayout(FaderGroupToAdd, RowIndex, ColumnIndex);
		ActiveLayout->AddToActiveFaderGroups(FaderGroupToAdd);

		FaderGroupToAdd->Modify();
		FaderGroupToAdd->SetIsActive(true);
		FaderGroupToAdd->SetIsExpanded(FirstSelectedFaderGroup->IsExpanded());

		FaderGroupsToSelect.Add(FaderGroupToAdd);

		ColumnIndex++;
	}

	// Remove all Selected Fader Groups from layout
	TArray<UObject*> FaderGroupsToUnselect;
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	for (const TWeakObjectPtr<UObject> SelectedFaderGroupObject : SelectedFaderGroupsObjects)
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectedFaderGroupObject.IsValid() ? Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject.Get()) : nullptr;
		if (!SelectedFaderGroup)
		{
			continue;
		}

		ActiveLayout->RemoveFromLayout(SelectedFaderGroup);
		ActiveLayout->RemoveFromActiveFaderGroups(SelectedFaderGroup);
		if (!SelectedFaderGroup->HasFixturePatch())
		{
			SelectedFaderGroup->Destroy();
		}

		FaderGroupsToUnselect.Add(SelectedFaderGroup);
	}

	ActiveLayout->ClearEmptyLayoutRows();
	ActiveLayout->PostEditChange();

	constexpr bool bNotifySelectionChange = false;
	SelectionHandler->AddToSelection(FaderGroupsToSelect, bNotifySelectionChange);
	SelectionHandler->RemoveFromSelection(FaderGroupsToUnselect);

	EditorModel->RequestUpdateEditorModel();
}

#undef LOCTEXT_NAMESPACE
