// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "DMXControlConsoleEditorGlobalLayoutBase.generated.h"

class UDMXControlConsoleData;
class UDMXControlConsoleEditorGlobalLayoutRow;
class UDMXControlConsoleFaderGroup;
class UDMXEntity;
class UDMXEntityFixturePatch;
class UDMXLibrary;


/** Enum for DMX Control Console layout modes */
UENUM()
enum class EDMXControlConsoleLayoutMode : uint8
{
	Horizontal,
	Vertical,
	Grid,
	None
};

/** Base class for the Control Console layout */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutBase
	: public UObject
{
	GENERATED_BODY()

public:
	/** Adds the given Fader Group to the layout at the given row/column index */
	void AddToLayout(UDMXControlConsoleFaderGroup* FaderGroup, const int32 RowIndex, const int32 ColumnIndex = INDEX_NONE);

	/** Adds a new Layout Row at the given index */
	UDMXControlConsoleEditorGlobalLayoutRow* AddNewRowToLayout(const int32 RowIndex = INDEX_NONE);

	/** Removes the given Fader Group from the layout */
	void RemoveFromLayout(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Gets an array of all the Layout Rows in this layout */
	const TArray<UDMXControlConsoleEditorGlobalLayoutRow*>& GetLayoutRows() const { return LayoutRows; }

	/** Gets the Layout Row which owns the given Fader Group, if valid */
	UDMXControlConsoleEditorGlobalLayoutRow* GetLayoutRow(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets an array of all the Fader Groups in this layout */
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> GetAllFaderGroups() const;

	/** Adds the Fader Group to the array of active Fader Groups */
	void AddToActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Removes the Fader Group form the array of active Fader Groups */
	void RemoveFromActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Gets an array of all the active Fader Groups in this layout */
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> GetAllActiveFaderGroups() const; 

	/** Sets the activity state of all the Fader Groups in the layout */
	void SetActiveFaderGroupsInLayout(bool bActive);

	/** Gets the row index of the given Fader Group, if valid */
	int32 GetFaderGroupRowIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets the column index of the given Fader Group, if valid */
	int32 GetFaderGroupColumnIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Gets the current layout mode */
	EDMXControlConsoleLayoutMode GetLayoutMode() const { return LayoutMode; }

	/** Sets the current layout mode */
	void SetLayoutMode(const EDMXControlConsoleLayoutMode NewLayoutMode);

	/** True if the layout contains the given Fader Group */
	bool ContainsFaderGroup(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Generates Layout Rows by the given Control Console Data */
	void GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData);

	/** Finds a patched Fader Group in the layout */
	UDMXControlConsoleFaderGroup* FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const;

	/** Clears all Layout Rows */
	void ClearAll(const bool bOnlyPatchedFaderGroups = false);

	/** Clears all empty Layout Rows in the layout */
	void ClearEmptyLayoutRows();

	/** Registers this layout */
	void Register(UDMXControlConsoleData* ControlConsoleData);

	/** Unregisters this layout */
	void Unregister(UDMXControlConsoleData* ControlConsoleData);

	/** True if this layout is registered to the DMX Library delegates */
	bool IsRegistered() const { return bIsRegistered; }

	// Property Name getters
	FORCEINLINE static FName GetLayoutModePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutBase, LayoutMode); }
	FORCEINLINE static FName GetLayoutNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutBase, LayoutName); }

	/** Name identifier of this Layout */
	UPROPERTY()
	FString LayoutName;

protected:
	//~ Begin UObject interface
	virtual void BeginDestroy() override;
	//~ End UObject interface

private:
	/** Called when a Fixture Patch was removed from a DMX Library */
	void OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities);

	/** Called when a Fader Group was added to Control Console Data */
	void OnFaderGroupAddedToData(const UDMXControlConsoleFaderGroup* FaderGroup, UDMXControlConsoleData* ControlConsoleData);

	/** Called to clean this layout from all the unpatched Fader Groups */
	void CleanLayoutFromUnpatchedFaderGroups();

	/** Reference to the Layout Rows array */
	UPROPERTY()
	TArray<TObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow>> LayoutRows;

	/** Array of the currently active Fader Groups in the layout */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> ActiveFaderGroups;

	/** Current layout sorting method for this layout */
	UPROPERTY()
	EDMXControlConsoleLayoutMode LayoutMode = EDMXControlConsoleLayoutMode::Grid;

	/** True if the layout is registered to the DMX Library delegates */
	bool bIsRegistered = false;
};
