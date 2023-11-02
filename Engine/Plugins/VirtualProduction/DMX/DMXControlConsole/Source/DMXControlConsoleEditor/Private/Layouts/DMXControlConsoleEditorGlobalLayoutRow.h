// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"

#include "DMXControlConsoleEditorGlobalLayoutRow.generated.h"

class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleFaderGroup;


/** A row of Fader Groups in the Control Console Global Layout */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutRow
	: public UObject
{
	GENERATED_BODY()

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGlobalLayoutRowChangedDelegate, UDMXControlConsoleEditorGlobalLayoutRow* /** ChangedRow */)
	
public:
	/** Adds the given Fader Group to the Layout Row at the given index */
	void AddToLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup, const int32 Index = INDEX_NONE);

	/** Adds the given array of Fader Groups to the Layout Row */
	void AddToLayoutRow(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups);

	/** Removes the given Fader Group from the Layout Row */
	void RemoveFromLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup);

	/** Gets Fader Groups array for this row */
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& GetFaderGroups() const { return FaderGroups; }

	/** Gets the Index of this row according to the owner layout */
	int32 GetRowIndex() const;

	/** Gets the layout that owns this row */
	UDMXControlConsoleEditorGlobalLayoutBase& GetOwnerLayoutChecked() const;

	/** Gets the Fader Group at the given index, if valid */
	UDMXControlConsoleFaderGroup* GetFaderGroupAt(const int32 Index) const { return FaderGroups.IsValidIndex(Index) ? FaderGroups[Index].Get() : nullptr; }

	/** Gets index of the given Fader Group, if valid */
	int32 GetIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const;

	/** Returns a delegate broadcast if a layout row has changed */
	static FOnGlobalLayoutRowChangedDelegate& GetOnGlobalLayoutRowChanged() { return OnGlobalLayoutRowChanged; };

private:
	/** Reference to the Fader Groups array */
	UPROPERTY()
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups;

	/** Delegate raised when the layout row has changed */
	static FOnGlobalLayoutRowChangedDelegate OnGlobalLayoutRowChanged;
};

