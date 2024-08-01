// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageExpression.h"
#include "Templates/SharedPointerFwd.h"

class SDMMaterialSlotEditor;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UToolMenu;
enum class EDMMaterialPropertyType : uint8;

class FDMMaterialSlotLayerMenus final
{
public:
	static UToolMenu* GenerateSlotLayerMenu(const TSharedPtr<SDMMaterialSlotEditor>& InSlotWidget, UDMMaterialLayerObject* InLayerObject);

	static void AddAddLayerSection(UToolMenu* InMenu);

private:
	static void AddLayerModifySection(UToolMenu* InMenu);

	static void AddLayerAddEffectsSection(UToolMenu* InMenu, UDMMaterialLayerObject* InLayerObject);

	static void AddGlobalValueSection(UToolMenu* InMenu);

	static void AddSlotMenuEntry(const TSharedPtr<SDMMaterialSlotEditor> InSlotWidget, UToolMenu* InMenu, const FText& InName, 
		UDMMaterialSlot* InSourceSlot, EDMMaterialPropertyType InMaterialProperty);

	static void AddLayerInputsMenu_Slot_Properties(UToolMenu* InMenu, UDMMaterialSlot* InSlot);

	static void AddLayerInputsMenu_Slots(UToolMenu* InMenu);

	static void AddLayerMenu_Gradients(UToolMenu* InMenu);
};
