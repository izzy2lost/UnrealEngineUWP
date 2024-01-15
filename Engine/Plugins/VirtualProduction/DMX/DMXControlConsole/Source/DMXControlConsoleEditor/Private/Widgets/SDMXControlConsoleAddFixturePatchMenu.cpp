// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleAddFixturePatchMenu.h"

#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
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
			!CurrentLayout->GetAllFaderGroupControllers().IsEmpty() &&
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
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
	if (!SelectedFaderGroupControllersObjects.IsEmpty())
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController(true);
		RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController);
		ColumnIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(SelectedFaderGroupController);
	}

	const FScopedTransaction AddToLastRowTransaction(LOCTEXT("AddToLastRowTransaction", "Add Fader Group"));
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

		if (ActiveLayout->ContainsFaderGroup(FaderGroup))
		{
			continue;
		}

		if (ColumnIndex != INDEX_NONE)
		{
			ColumnIndex++;
		}

		ActiveLayout->PreEditChange(nullptr);
		UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(FaderGroup, FaderGroup->GetFaderGroupName(), RowIndex, ColumnIndex);
		ActiveLayout->PostEditChange();
		if (NewController)
		{
			NewController->Modify();
			NewController->SetIsActive(true);
			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}
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
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
	if (SelectedFaderGroupControllersObjects.IsEmpty())
	{
		NewRowIndex = ActiveLayout->GetLayoutRows().Num();
	}
	else
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController(true);
		if (!SelectedFaderGroupController)
		{
			return;
		}

		NewRowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(SelectedFaderGroupController) + 1;
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

			if (ActiveLayout->ContainsFaderGroup(FaderGroup))
			{
				continue;
			}

			UDMXControlConsoleFaderGroupController* NewController = NewLayoutRow->CreateFaderGroupController(FaderGroup, FaderGroup->GetFaderGroupName());
			if (NewController)
			{
				NewController->Modify();
				NewController->SetIsActive(true);
				ActiveLayout->AddToActiveFaderGroupControllers(NewController);
			}
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
	const UDMXControlConsoleFaderGroupController* FirstSelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController();
	if (!FirstSelectedFaderGroupController)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	const int32 RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(FirstSelectedFaderGroupController);

	const FScopedTransaction ReplaceSelectedFaderGroupTransaction(LOCTEXT("ReplaceSelectedFaderGroupTransaction", "Replace Fader Group"));
	ActiveLayout->PreEditChange(nullptr);

	// Add all Selected Patches Fader Group Controllers to layout
	TArray<UObject*> FaderGroupControllersToSelect;
	int32 ColumnIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(FirstSelectedFaderGroupController);
	for (TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch : FixturePatches)
	{
		const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
		if (!FixturePatch)
		{
			continue;
		}

		UDMXControlConsoleFaderGroup* FaderGroupToAdd = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		if (!FaderGroupToAdd)
		{
			continue;
		}

		UDMXControlConsoleFaderGroupController*	NewController = ActiveLayout->AddToLayout(FaderGroupToAdd, FaderGroupToAdd->GetFaderGroupName(), RowIndex, ColumnIndex);
		if (NewController)
		{
			NewController->Modify();
			NewController->SetIsActive(true);
			NewController->SetIsExpanded(FirstSelectedFaderGroupController->IsExpanded());

			ActiveLayout->AddToActiveFaderGroupControllers(NewController);
		}

		FaderGroupControllersToSelect.Add(NewController);

		ColumnIndex++;
	}

	// Remove all Selected Fader Group Controllers from layout
	TArray<UObject*> FaderGroupControllersToUnselect;
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
	for (const TWeakObjectPtr<UObject> SelectedFaderGroupControllerObject : SelectedFaderGroupControllersObjects)
	{
		UDMXControlConsoleFaderGroupController* SelectedFaderGroupController =  Cast<UDMXControlConsoleFaderGroupController>(SelectedFaderGroupControllerObject.Get());
		if (!SelectedFaderGroupController)
		{
			continue;
		}

		FaderGroupControllersToUnselect.Add(SelectedFaderGroupController);
		if (SelectedFaderGroupController->HasFixturePatch())
		{
			continue;
		}

		// Destroy all unpatched fader groups in the controller
		SelectedFaderGroupController->Modify();
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = SelectedFaderGroupController->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid())
			{
				SelectedFaderGroupController->UnPossess(FaderGroup.Get());

				FaderGroup->Modify();
				FaderGroup->Destroy();
			}
		}

		ActiveLayout->RemoveFromActiveFaderGroupControllers(SelectedFaderGroupController);
		SelectedFaderGroupController->Destroy();
	}

	ActiveLayout->ClearEmptyLayoutRows();
	ActiveLayout->PostEditChange();

	constexpr bool bNotifySelectionChange = false;
	SelectionHandler->AddToSelection(FaderGroupControllersToSelect, bNotifySelectionChange);
	SelectionHandler->RemoveFromSelection(FaderGroupControllersToUnselect);

	EditorModel->RequestUpdateEditorModel();
}

#undef LOCTEXT_NAMESPACE
