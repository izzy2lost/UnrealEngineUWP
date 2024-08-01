// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class FName;
class SDMMaterialSlotEditor;
class SDMMaterialStage;
class UToolMenu;

class FDMMaterialStageMenus final
{
public:
	static UToolMenu* GenerateStageMenu(const TSharedPtr<SDMMaterialSlotEditor>& InSlotWidget, const TSharedPtr<SDMMaterialStage>& InStageWidget);

private:
	static FName GetStageMenuName();

	static FName GetStageToggleSectionName();

	static void AddStageSection(UToolMenu* InMenu);
};
