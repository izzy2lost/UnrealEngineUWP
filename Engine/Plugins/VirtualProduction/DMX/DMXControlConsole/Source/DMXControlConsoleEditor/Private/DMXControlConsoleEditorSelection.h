// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderBase;
class UDMXControlConsoleFaderGroup;


class FDMXControlConsoleEditorSelection final
	: public TSharedFromThis<FDMXControlConsoleEditorSelection>
{
public:
	DECLARE_EVENT(FDMXControlConsoleEditorSelection, FDMXControlConsoleSelectionEvent)

	/** Constructor */
	FDMXControlConsoleEditorSelection(UDMXControlConsoleEditorModel* InEditorModel);

	/** Adds the given Fader Group to the selection */
	void AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Adds the given Fader to the selection */
	void AddToSelection(UDMXControlConsoleFaderBase* Fader, bool bNotifySelectionChange = true);

	/** Adds the elements in the given array to the selection */
	void AddToSelection(const TArray<UObject*> Elements, bool bNotifySelectionChange = true);

	/** Adds to the selection all the Faders from the given Fader Group */
	void AddAllFadersFromFaderGroupToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bOnlyMatchingFilter = false, bool bNotifySelectionChange = true);

	/** Removes the given Fader Group from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Removes the given Fader from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderBase* Fader, bool bNotifySelectionChange = true);

	/** Removes the elements in the given array from selection */
	void RemoveFromSelection(const TArray<UObject*> Elements, bool bNotifySelectionChange = true);

	/** Multiselects the Fader or Fader Group and the current selection */
	void Multiselect(UObject* FaderOrFaderGroupObject);

	/** Replaces the given selected Fader Group with the next available one */
	void ReplaceInSelection(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Replaces the given selected Fader with the next available one */
	void ReplaceInSelection(UDMXControlConsoleFaderBase* Fader);

	/** Gets wheter the given Fader Group is selected or not */
	bool IsSelected(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets wheter the given Fader is selected or not */
	bool IsSelected(UDMXControlConsoleFaderBase* Fader) const;

	/** Selects all the Fader Groups and Faders in the current Control Console Data */
	void SelectAll(bool bOnlyMatchingFilter = false);

	/** Removes all the invalid object in the selected objects arrays */
	void RemoveInvalidObjectsFromSelection(bool bNotifySelectionChange = true);

	/** Clears all Faders owned by the given FadernGroup from the selection */
	void ClearFadersSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Clears all the selected objects arrays */
	void ClearSelection(bool bNotifySelectionChange = true);

	/** Gets the SelectedFaderGorups array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaderGroups() const { return SelectedFaderGroups; }

	/** Gets the first selected Fader Group sorted by index */
	UDMXControlConsoleFaderGroup* GetFirstSelectedFaderGroup(bool bReverse = false) const;

	/** Gets the SelectedFaders array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaders() const { return SelectedFaders; }

	/** Gets the first selected Fader sorted by index */
	UDMXControlConsoleFaderBase* GetFirstSelectedFader(bool bReverse = false) const;

	/** Gets all the selected Faders from the given Fader Group */
	TArray<UDMXControlConsoleFaderBase*> GetSelectedFadersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Returns an event raised when the selection has changed */
	FDMXControlConsoleSelectionEvent& GetOnSelectionChanged() { return OnSelectionChanged; }

private:
	/** Updates the multi select anchor */
	void UpdateMultiSelectAnchor(UClass* PreferedClass);

	/** Called whenever the current selection changes */
	FDMXControlConsoleSelectionEvent OnSelectionChanged;

	/** Array of the current selected Fader Groups */
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroups;

	/** Array of the current selected Faders */
	TArray<TWeakObjectPtr<UObject>> SelectedFaders;

	/** Anchor while multi selecting */
	TWeakObjectPtr<UObject> MultiSelectAnchor;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
};
