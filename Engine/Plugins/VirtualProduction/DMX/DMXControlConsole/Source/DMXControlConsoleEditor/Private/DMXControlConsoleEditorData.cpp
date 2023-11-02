// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorData.h"


void UDMXControlConsoleEditorData::SetFaderGroupsViewMode(EDMXControlConsoleEditorViewMode ViewMode)
{
	FaderGroupsViewMode = ViewMode;
	OnFaderGroupsViewModeChanged.Broadcast();
}

void UDMXControlConsoleEditorData::SetFadersViewMode(EDMXControlConsoleEditorViewMode ViewMode)
{
	FadersViewMode = ViewMode;
	OnFadersViewModeChanged.Broadcast();
}

void UDMXControlConsoleEditorData::ToggleAutoSelectActivePatches()
{
	bAutoSelectActivePatches = !bAutoSelectActivePatches;
}

void UDMXControlConsoleEditorData::ToggleAutoSelectFilteredElements()
{
	bAutoSelectFilteredElements = !bAutoSelectFilteredElements;
}
