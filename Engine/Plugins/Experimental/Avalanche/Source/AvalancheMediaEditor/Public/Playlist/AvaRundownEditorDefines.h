// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownEditorDefines.generated.h"

/** Defines the page set that an action applies to. */
UENUM()
enum class EAvaRundownPageSet : uint8
{
	/**
	 * The action applies to either the selected pages or playing pages if no pages are selected.
	 * If the selection is not empty and has no applicable pages, then the action is disabled.
	 */
	SelectedOrPlayingStrict,
	
	/**
	 * The action applies to applicable selected pages or playing pages if no applicable pages are selected.
	 * The selection may have pages, but if not applicable to the action, playing pages are used instead.
	 */
	SelectedOrPlaying,
	
	/**
	 * The action applies to selected pages only.
	 * For actions requiring the page to be playing, this effectively becomes "selected and playing".
	 */
	Selected,
	
	/** The action applies to playing pages only regardless of selection. */
	Playing
};
