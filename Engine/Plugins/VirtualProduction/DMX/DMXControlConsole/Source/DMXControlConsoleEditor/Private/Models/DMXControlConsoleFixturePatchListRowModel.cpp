// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleFixturePatchListRowModel.h"

#include "DMXControlConsoleFaderGroup.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "ScopedTransaction.h"
#include "Styling/SlateTypes.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFixturePatchListRowModel"

FDMXControlConsoleFixturePatchListRowModel::FDMXControlConsoleFixturePatchListRowModel(TWeakObjectPtr<UDMXEntityFixturePatch> InFixturePatch)
	: WeakFixturePatch(InFixturePatch)
{}

bool FDMXControlConsoleFixturePatchListRowModel::IsRowEnabled() const
{
	const UDMXEntityFixturePatch* FixturePatch = WeakFixturePatch.Get();
	if (!FixturePatch)
	{
		return true;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts)
	{
		return true;
	}

	// Do only if active layout is not default Layout
	const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return true;
	}

	if (ActiveLayout == &EditorConsoleLayouts->GetDefaultLayoutChecked())
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

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		const UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
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

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	if (UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		if (UDMXControlConsoleFaderGroup* FaderGroup = EditorConsoleData->FindFaderGroupByFixturePatch(FixturePatch))
		{
			const FText TransactionText = bMuted ?
				LOCTEXT("MuteFaderGroupTransaction", "Mute Fader Group") :
				LOCTEXT("UnmuteFaderGroupTransaction", "Unmute Fader Group");
			const FScopedTransaction SetFaderGroupMutedTransaction(TransactionText);

			FaderGroup->Modify();
			FaderGroup->SetMute(bMuted);
		}
	}
}

#undef LOCTEXT_NAMESPACE 

