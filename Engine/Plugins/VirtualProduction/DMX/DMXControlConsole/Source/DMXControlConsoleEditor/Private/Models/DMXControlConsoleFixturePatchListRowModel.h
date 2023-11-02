// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

enum class ECheckBoxState : uint8;
class UDMXControlConsoleEditorModel;
class UDMXEntityFixturePatch;


/** Model for a row in the control console fixture patch list */
class FDMXControlConsoleFixturePatchListRowModel
	: public TSharedFromThis<FDMXControlConsoleFixturePatchListRowModel>
{
public:
	FDMXControlConsoleFixturePatchListRowModel(const TWeakObjectPtr<UDMXEntityFixturePatch> InWeakFixturePatch, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

	/** Returns true if the row widget is enabled */
	bool IsRowEnabled() const;

	/** Returns the fixture group muted state. Checked means it is not muted. */
	ECheckBoxState GetFaderGroupMutedState() const;

	/** Sets if the fixture group is muted. */
	void SetFaderGroupMuted(bool bEnabled);

private:
	/** The fixture patch of this row */
	TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch;

	/** Weak reference to the Control Console edior model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
};
