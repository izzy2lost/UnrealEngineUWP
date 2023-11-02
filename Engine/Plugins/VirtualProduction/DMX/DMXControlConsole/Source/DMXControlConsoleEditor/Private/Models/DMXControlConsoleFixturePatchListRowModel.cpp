// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchListRowModel.h"

#include "DMXControlConsoleFaderGroup.h"
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

	const UDMXControlConsoleFaderGroup* FaderGroup = ActiveLayout->FindFaderGroupByFixturePatch(FixturePatch);
	return !IsValid(FaderGroup);
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

	if (const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData())
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
		return IsValid(FaderGroup) && FaderGroup->IsMuted() ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
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

	UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
	if (!ControlConsoleData)
	{
		return;
	}

	if (UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch))
	{
		const FText TransactionText = bMuted ?
			LOCTEXT("MuteFaderGroupTransaction", "Mute Fader Group") :
			LOCTEXT("UnmuteFaderGroupTransaction", "Unmute Fader Group");
		const FScopedTransaction SetFaderGroupMutedTransaction(TransactionText);

		FaderGroup->Modify();
		FaderGroup->SetMute(bMuted);
	}
}

#undef LOCTEXT_NAMESPACE 

