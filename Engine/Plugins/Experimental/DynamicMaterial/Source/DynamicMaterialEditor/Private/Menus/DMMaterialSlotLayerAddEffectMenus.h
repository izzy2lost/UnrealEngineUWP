// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPtr.h"

class SDMEditor;
class SWidget;
class UDMMaterialLayerObject;
class UMaterialFunctionInterface;
class UToolMenu;
struct FToolMenuContext;

class FDMMaterialSlotLayerAddEffectMenus
{
public:
	static TSharedRef<SWidget> OpenAddEffectMenu(const TSharedPtr<SDMEditor>& InEditor, UDMMaterialLayerObject* InLayer);

	static void AddEffectSubMenu(UToolMenu* InMenu, UDMMaterialLayerObject* InLayer);
};
