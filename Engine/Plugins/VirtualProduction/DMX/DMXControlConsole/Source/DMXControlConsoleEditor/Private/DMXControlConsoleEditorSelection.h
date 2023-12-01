// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDMXControlConsoleFaderGroupElement.h"
#include "UObject/WeakObjectPtr.h"

class IDMXControlConsoleFaderGroupElement;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderGroup;


/** Class for handling the selection in the DMX Control Console */
class FDMXControlConsoleEditorSelection final
	: public TSharedFromThis<FDMXControlConsoleEditorSelection>
{
public:
	DECLARE_EVENT(FDMXControlConsoleEditorSelection, FDMXControlConsoleSelectionEvent)

	/** Constructor */
	FDMXControlConsoleEditorSelection(UDMXControlConsoleEditorModel* InEditorModel);

	/** Adds the given Fader Group to the selection */
	void AddToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Adds the given Element Controller to the selection */
	void AddToSelection(UDMXControlConsoleElementController* ElementController, bool bNotifySelectionChange = true);

	/** Adds the objects in the given array to the selection */
	void AddToSelection(const TArray<UObject*> Objects, bool bNotifySelectionChange = true);

	/** Adds to the selection all the Faders from the given Fader Group */
	void AddAllFadersFromFaderGroupToSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bOnlyMatchingFilter = false, bool bNotifySelectionChange = true);

	/** Removes the given Fader Group from selection */
	void RemoveFromSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Removes the given Element Controller from selection */
	void RemoveFromSelection(UDMXControlConsoleElementController* ElementController, bool bNotifySelectionChange = true);

	/** Removes the objects in the given array from selection */
	void RemoveFromSelection(const TArray<UObject*> Objects, bool bNotifySelectionChange = true);

	/** Multiselects the Element Controller or Fader Group in the current selection */
	void Multiselect(UObject* ElementControllerOrFaderGroupObject);

	/** Replaces the given selected Fader Group with the next available one */
	void ReplaceInSelection(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Replaces the given selected Element Controller with the next available one */
	void ReplaceInSelection(UDMXControlConsoleElementController* ElementController);

	/** Gets wheter the given Fader Group is selected or not */
	bool IsSelected(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets wheter the given Element Controller is selected or not */
	bool IsSelected(UDMXControlConsoleElementController* ElementController) const;

	/** Selects all the Fader Groups and Element Controllers in the current Control Console Data */
	void SelectAll(bool bOnlyMatchingFilter = false);

	/** Removes all the invalid object in the selected objects arrays */
	void RemoveInvalidObjectsFromSelection(bool bNotifySelectionChange = true);

	/** Clears all Element Controllers owned by the given FadernGroup from the selection */
	void ClearElementControllersSelection(UDMXControlConsoleFaderGroup* FaderGroup, bool bNotifySelectionChange = true);

	/** Clears all the selected objects arrays */
	void ClearSelection(bool bNotifySelectionChange = true);

	/** Gets the SelectedFaderGorups array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedFaderGroups() const { return SelectedFaderGroups; }

	/** Gets the first selected Fader Group sorted by index */
	UDMXControlConsoleFaderGroup* GetFirstSelectedFaderGroup(bool bReverse = false) const;

	/** Gets the SelectedElementControllers array */
	TArray<TWeakObjectPtr<UObject>> GetSelectedElementControllers() const { return SelectedElementControllers; }

	/** Gets the first selected Element Controller sorted by index */
	UDMXControlConsoleElementController* GetFirstSelectedElementController(bool bReverse = false) const;

	/** Gets all the selected Element Controller from the given Fader Group */
	TArray<UDMXControlConsoleElementController*> GetSelectedElementControllersFromFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets all the selected Elements from the selected Element Controllers array */
	TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> GetSelectedElements(bool bSort = false) const;

	/** Returns an event raised when the selection has changed */
	FDMXControlConsoleSelectionEvent& GetOnSelectionChanged() { return OnSelectionChanged; }

private:
	/** Updates the multi select anchor */
	void UpdateMultiSelectAnchor(UClass* PreferedClass);

	/** Called whenever the current selection changes */
	FDMXControlConsoleSelectionEvent OnSelectionChanged;

	/** Array of the current selected Fader Groups */
	TArray<TWeakObjectPtr<UObject>> SelectedFaderGroups;

	/** Array of the current selected Element Controllers */
	TArray<TWeakObjectPtr<UObject>> SelectedElementControllers;

	/** Anchor while multi selecting */
	TWeakObjectPtr<UObject> MultiSelectAnchor;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
};
