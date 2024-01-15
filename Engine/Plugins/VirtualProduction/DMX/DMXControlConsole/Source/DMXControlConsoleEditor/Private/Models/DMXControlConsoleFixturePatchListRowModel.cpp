// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchListRowModel.h"

#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Styling/SlateTypes.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchListRowModel"

FDMXControlConsoleFixturePatchListRowModel::FDMXControlConsoleFixturePatchListRowModel(const TWeakObjectPtr<UDMXEntityFixturePatch> InWeakFixturePatch, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
	: WeakFixturePatch(InWeakFixturePatch)
	, WeakEditorModel(InWeakEditorModel)
{}

bool FDMXControlConsoleFixturePatchListRowModel::IsRowEnabled() const
{
	const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
	if (!FixturePatch)
	{
		return true;
	}

	const UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
	if (!EditorModel)
	{
		return true;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return true;
	}

	// Do only if active layout is not default Layout
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return true;
	}

	if (ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
	{
		return true;
	}

	const UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch);
	return !IsValid(FaderGroupController);
}

ECheckBoxState FDMXControlConsoleFixturePatchListRowModel::GetFaderGroupMutedState() const
{	
	const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
	if (!FixturePatch)
	{
		// Fixture groups shown in the list always have a fixture patch
		return ECheckBoxState::Undetermined;
	}

	const UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
	if (!EditorModel)
	{
		return ECheckBoxState::Undetermined;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (ActiveLayout)
	{
		const UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch);
		return IsValid(FaderGroupController) && FaderGroupController->IsMuted() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
	}

	return ECheckBoxState::Undetermined;
}

void FDMXControlConsoleFixturePatchListRowModel::SetFaderGroupMuted(bool bMuted)
{
	const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
	if (!FixturePatch)
	{
		return;
	}

	const UDMXControlConsoleEditorModel* EditorModel = WeakEditorModel.Get();
	if (!EditorModel)
	{
		return;
	}

	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
	if (!ActiveLayout)
	{
		return;
	}

	if (UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch))
	{
		const FText TransactionText = bMuted ?
			LOCTEXT("MuteFaderGroupTransaction", "Mute Fader Group") :
			LOCTEXT("UnmuteFaderGroupTransaction", "Unmute Fader Group");
		const FScopedTransaction SetFaderGroupMutedTransaction(TransactionText);

		FaderGroupController->Modify();
		FaderGroupController->SetMute(bMuted);
	}
}

#undef LOCTEXT_NAMESPACE 
