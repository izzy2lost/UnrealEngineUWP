// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMMenuContext.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageSource.h"
#include "Model/DynamicMaterialModel.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMStage.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Slate/SDMSlot.h"

UDMMenuContext* UDMMenuContext::Create(const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject)
{
	UDMMenuContext* Context = NewObject<UDMMenuContext>();
	Context->EditorWidgetWeak = InEditorWidget;
	Context->StageWidgetWeak = InStageWidget;
	Context->LayerObjectWeak = InLayerObject;
	return Context;
}

UToolMenu* UDMMenuContext::GenerateContextMenu(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject)
{
	UToolMenu* NewMenu = UToolMenus::Get()->RegisterMenu(InMenuName, NAME_None, EMultiBoxType::Menu, false);
	if (!NewMenu)
	{
		return nullptr;
	}

	if (!IsValid(InLayerObject))
	{
		if (TSharedPtr<SDMStage> StageWidget = InStageWidget.Pin())
		{
			if (UDMMaterialStage* Stage = StageWidget->GetStage())
			{
				InLayerObject = Stage->GetLayer();
			}
		}
	}

	NewMenu->bToolBarForceSmallIcons = true;
	NewMenu->bShouldCloseWindowAfterMenuSelection = true;
	NewMenu->bCloseSelfOnly = true;

	TSharedPtr<FUICommandList> CommandList = nullptr;

	if (TSharedPtr<SDMEditor> Editor = InEditorWidget.Pin())
	{
		CommandList = Editor->GetCommandList();
	}

	NewMenu->Context = FToolMenuContext(CommandList, TSharedPtr<FExtender>(), Create(InEditorWidget, InStageWidget, InLayerObject));

	return NewMenu;
}

UDMMenuContext* UDMMenuContext::CreateEmpty()
{
	return Create(nullptr, nullptr, nullptr);
}

UDMMenuContext* UDMMenuContext::CreateEditor(const TWeakPtr<SDMEditor>& InEditorWidget)
{
	return Create(InEditorWidget, nullptr, nullptr);
}

UDMMenuContext* UDMMenuContext::CreateLayer(const TWeakPtr<SDMEditor>& InEditorWidget, UDMMaterialLayerObject* InLayerObject)
{
	return Create(InEditorWidget, nullptr, InLayerObject);
}

UDMMenuContext* UDMMenuContext::CreateStage(const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget)
{
	return Create(InEditorWidget, InStageWidget, nullptr);
}

UToolMenu* UDMMenuContext::GenerateContextMenuDefault(const FName InMenuName)
{
	return GenerateContextMenu(InMenuName, nullptr, nullptr, nullptr);
}

UToolMenu* UDMMenuContext::GenerateContextMenuEditor(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget)
{
	return GenerateContextMenu(InMenuName, InEditorWidget, nullptr, nullptr);
}

UToolMenu* UDMMenuContext::GenerateContextMenuLayer(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget, UDMMaterialLayerObject* InLayerObject)
{
	return GenerateContextMenu(InMenuName, InEditorWidget, nullptr, InLayerObject);
}

UToolMenu* UDMMenuContext::GenerateContextMenuStage(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget)
{
	return GenerateContextMenu(InMenuName, InEditorWidget, InStageWidget, nullptr);
}

UDMMaterialSlot* UDMMenuContext::GetSlot() const
{
	if (TSharedPtr<SDMEditor> EditorWidget = EditorWidgetWeak.Pin())
	{
		if (TSharedPtr<SDMSlot> SlotWidget = EditorWidget->GetActiveSlotWidget())
		{
			return SlotWidget->GetSlot();
		}
	}

	return nullptr;;
}

UDynamicMaterialModel* UDMMenuContext::GetModel() const
{
	if (TSharedPtr<SDMEditor> EditorWidget = EditorWidgetWeak.Pin())
	{
		return EditorWidget->GetMaterialModel();
	}

	return nullptr;
}

UDMMaterialStage* UDMMenuContext::GetStage() const
{
	if (TSharedPtr<SDMStage> StageWidget = StageWidgetWeak.Pin())
	{
		return StageWidget->GetStage();
	}

	return nullptr;
}

UDMMaterialStageSource* UDMMenuContext::GetStageSource() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		return Stage->GetSource();
	}

	return nullptr;
}

UDMMaterialStageBlend* UDMMenuContext::GetStageSourceAsBlend() const
{
	return Cast<UDMMaterialStageBlend>(GetStageSource());
}

const UDMMaterialLayerObject* UDMMenuContext::GetLayer() const
{
	return LayerObjectWeak.Get();
}
