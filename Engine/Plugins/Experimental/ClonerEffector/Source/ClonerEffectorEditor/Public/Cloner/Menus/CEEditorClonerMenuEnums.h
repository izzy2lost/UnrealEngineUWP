// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

/** Menu type available */
enum class ECEEditorClonerMenuType : uint8
{
	Enable = 1 << 0,
	Disable = 1 << 1,
	CreateEffector = 1 << 2,
	Convert = 1 << 3
};