// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXControlConsoleEditorDataBase.h"
#include "Widgets/SDMXReadOnlyFixturePatchList.h"

#include "DMXControlConsoleEditorData.generated.h"


/** Enum for DMX Control Console control modes */
UENUM()
enum class EDMXControlConsoleEditorControlMode : uint8
{
	Relative,
	Absolute
};

/** Enum for DMX Control Console view modes */
UENUM()
enum class EDMXControlConsoleEditorViewMode : uint8
{
	Collapsed,
	Expanded
};

/** Control Console container class for editor data */
UCLASS()
class UDMXControlConsoleEditorData
	: public UDMXControlConsoleEditorDataBase
{
	GENERATED_BODY()

public:
	/** Gets the current Control Mode for Faders. */
	EDMXControlConsoleEditorControlMode GetControlMode() const { return ControlMode; }

	/** Sets the current Control Mode for Faders. */
	void SetControlMode(EDMXControlConsoleEditorControlMode NewControlMode) { ControlMode = NewControlMode; }

	/** Gets the current View Mode for Fader Groups. */
	EDMXControlConsoleEditorViewMode GetFaderGroupsViewMode() const { return FaderGroupsViewMode; }

	/** Sets the current View Mode for Fader Groups. */
	void SetFaderGroupsViewMode(EDMXControlConsoleEditorViewMode ViewMode);

	/** Gets the current View Mode for Faders. */
	EDMXControlConsoleEditorViewMode GetFadersViewMode() const { return FadersViewMode; }

	/** Sets the current View Mode for Faders. */
	void SetFadersViewMode(EDMXControlConsoleEditorViewMode ViewMode);

	/** Gets the current auto-selection state for the activated Fader Groups. */
	bool GetAutoSelectActivePatches() const { return bAutoSelectActivePatches; }

	/** Gets the current auto-selection state for the filtered Elements. */
	bool GetAutoSelectFilteredElements() const { return bAutoSelectFilteredElements; }

	/** Toggles the auto-selection state for the activated Fader Groups. */
	void ToggleAutoSelectActivePatches();

	/** Toggles the auto-selection state for the filtered Elements. */
	void ToggleAutoSelectFilteredElements();

	/** Returns a delegate broadcast whenever the Fader Groups view mode is changed */
	FSimpleMulticastDelegate& GetOnFaderGroupsViewModeChanged() { return OnFaderGroupsViewModeChanged; }

	/** Returns a delegate broadcast whenever the Faders view mode is changed */
	FSimpleMulticastDelegate& GetOnFadersViewModeChanged() { return OnFadersViewModeChanged; }

	/** Fixture Patch List default descriptor */
	UPROPERTY()
	FDMXReadOnlyFixturePatchListDescriptor FixturePatchListDescriptor;

private:
	/** Called when the Fader Groups view mode is changed */
	FSimpleMulticastDelegate OnFaderGroupsViewModeChanged;

	/** Called when the Faders view mode is changed */
	FSimpleMulticastDelegate OnFadersViewModeChanged;

	/** Current control mode for Faders widgets */
	UPROPERTY()
	EDMXControlConsoleEditorControlMode ControlMode = EDMXControlConsoleEditorControlMode::Absolute;

	/** Current view mode for FaderGroupView widgets*/
	UPROPERTY()
	EDMXControlConsoleEditorViewMode FaderGroupsViewMode = EDMXControlConsoleEditorViewMode::Expanded;

	/** Current view mode for Faders widgets */
	UPROPERTY()
	EDMXControlConsoleEditorViewMode FadersViewMode = EDMXControlConsoleEditorViewMode::Collapsed;

	UPROPERTY()
	/** True if the Fader Groups from activated Fixture Patches must be selected by default */
	bool bAutoSelectActivePatches = false;

	UPROPERTY()
	/** True if the filtered Elements must be selected by default */
	bool bAutoSelectFilteredElements = false;
};
