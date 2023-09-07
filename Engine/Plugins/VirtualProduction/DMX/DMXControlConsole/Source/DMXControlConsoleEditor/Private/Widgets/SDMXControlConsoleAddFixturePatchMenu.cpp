// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleAddFixturePatchMenu.h"

#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleAddFixturePatchMenu"

void SDMXControlConsoleAddFixturePatchMenu::Construct(const FArguments& InArgs, TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> InFixturePatches)
{
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

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleData && EditorConsoleLayouts)
	{
		// True if there's no global filter and no vertical sorting
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bCanExecute &=
			IsValid(CurrentLayout) &&
			CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Vertical &&
			!CurrentLayout->GetAllFaderGroups().IsEmpty() &&
			EditorConsoleData->FilterString.IsEmpty();
	}

	return bCanExecute;
}

void SDMXControlConsoleAddFixturePatchMenu::AddPatchesToTheRight()
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	int32 RowIndex = CurrentLayout->GetLayoutRows().Num() - 1;
	int32 ColumnIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (!SelectedFaderGroupsObjects.IsEmpty())
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup(true);
		RowIndex = CurrentLayout->GetFaderGroupRowIndex(SelectedFaderGroup);
		ColumnIndex = CurrentLayout->GetFaderGroupColumnIndex(SelectedFaderGroup) + 1;
	}

	// Add all selected Fixture Patches from Fixture Patch List
	for (const TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
	{
		UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		if (!FaderGroup)
		{
			continue;
		}

		if (CurrentLayout->GetAllFaderGroups().Contains(FaderGroup))
		{
			continue;
		}

		const FScopedTransaction AddToLastRowTransaction(LOCTEXT("AddToLastRowTransaction", "Add Fader Group"));
		CurrentLayout->PreEditChange(nullptr);
		if (ColumnIndex == INDEX_NONE)
		{
			CurrentLayout->AddToLayout(FaderGroup, RowIndex);
		}
		else
		{
			CurrentLayout->AddToLayout(FaderGroup, RowIndex, ColumnIndex);
			ColumnIndex++;
		}

		FaderGroup->Modify();
		FaderGroup->SetIsActive(true);

		CurrentLayout->PostEditChange();
	}
}

bool SDMXControlConsoleAddFixturePatchMenu::CanAddPatchesOnNewRow() const
{
	bool bCanExecute = !FixturePatches.IsEmpty();

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (EditorConsoleData && EditorConsoleLayouts)
	{
		// True if there's no global filter and no horizontal sorting
		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
		bCanExecute &=
			IsValid(CurrentLayout) &&
			CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Horizontal &&
			EditorConsoleData->FilterString.IsEmpty();
	}

	return bCanExecute;
}

void SDMXControlConsoleAddFixturePatchMenu::AddPatchesOnNewRow()
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	// Generate on last row if vertical sorting
	if (CurrentLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		AddPatchesToTheRight();
		return;
	}

	int32 NewRowIndex = INDEX_NONE;

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		NewRowIndex = CurrentLayout->GetLayoutRows().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup(true);
		if (!SelectedFaderGroup)
		{
			return;
		}

		NewRowIndex = CurrentLayout->GetFaderGroupRowIndex(SelectedFaderGroup) + 1;
	}

	const FScopedTransaction AddToNewtRowTransaction(LOCTEXT("AddToNewtRowTransaction", "Add Fader Group"));
	CurrentLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = CurrentLayout->AddNewRowToLayout(NewRowIndex);
	CurrentLayout->PostEditChange();

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

			UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			if (!FaderGroup)
			{
				continue;
			}

			if (CurrentLayout->GetAllFaderGroups().Contains(FaderGroup))
			{
				continue;
			}

			NewLayoutRow->AddToLayoutRow(FaderGroup);
			FaderGroup->Modify();
			FaderGroup->SetIsActive(true);
		}

		NewLayoutRow->PostEditChange();
	}
}

bool SDMXControlConsoleAddFixturePatchMenu::CanSetPatchOnFaderGroup() const
{
	bool bCanExecute = !FixturePatches.IsEmpty();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		// True if there's if there's no global filter and at least one selected Fader Group
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
		bCanExecute &=
			EditorConsoleData->FilterString.IsEmpty() &&
			!SelectionHandler->GetSelectedFaderGroups().IsEmpty();
	}

	return bCanExecute;
}

void SDMXControlConsoleAddFixturePatchMenu::SetPatchOnFaderGroup()
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const UDMXControlConsoleFaderGroup* FirstSelectedFaderGroup = SelectionHandler->GetFirstSelectedFaderGroup();
	if (!FirstSelectedFaderGroup)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	const int32 RowIndex = CurrentLayout->GetFaderGroupRowIndex(FirstSelectedFaderGroup);

	const FScopedTransaction ReplaceSelectedFaderGroupTransaction(LOCTEXT("ReplaceSelectedFaderGroupTransaction", "Replace Fader Group"));
	CurrentLayout->PreEditChange(nullptr);

	// Add all Selected Patches Fader Groups to layout
	TArray<UObject*> FaderGroupsToSelect;

	int32 ColumnIndex = CurrentLayout->GetFaderGroupColumnIndex(FirstSelectedFaderGroup);
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
	{
		const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroupToAdd = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		CurrentLayout->AddToLayout(FaderGroupToAdd, RowIndex, ColumnIndex);

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

		CurrentLayout->RemoveFromLayout(SelectedFaderGroup);
		if (!SelectedFaderGroup->HasFixturePatch())
		{
			SelectedFaderGroup->Destroy();
		}

		FaderGroupsToUnselect.Add(SelectedFaderGroup);
	}

	CurrentLayout->ClearEmptyLayoutRows();
	CurrentLayout->PostEditChange();

	constexpr bool bNotifySelectionChange = false;
	SelectionHandler->AddToSelection(FaderGroupsToSelect, bNotifySelectionChange);
	SelectionHandler->RemoveFromSelection(FaderGroupsToUnselect);

	EditorConsoleModel->RequestUpdateEditorModel();
}

#undef LOCTEXT_NAMESPACE
