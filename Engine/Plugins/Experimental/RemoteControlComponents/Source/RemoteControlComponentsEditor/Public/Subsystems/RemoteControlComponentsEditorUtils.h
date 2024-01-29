// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FName;
struct FSlateIcon;

class FRemoteControlComponentsEditorUtils
{
public:
	/** Retrieve the specified Icon from the Style associated to the Remote Control Components Editor */
	REMOTECONTROLCOMPONENTSEDITOR_API static FSlateIcon GetIcon(const FName& InIconName);
};
