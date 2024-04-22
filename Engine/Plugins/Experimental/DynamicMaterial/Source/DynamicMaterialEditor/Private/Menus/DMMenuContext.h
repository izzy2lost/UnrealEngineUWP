// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "DMMenuContext.generated.h"

class SDMEditor;
class SDMStage;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageBlend;
class UDMMaterialStageSource;
class UDynamicMaterialModel;
class UToolMenu;

UCLASS()
class UDMMenuContext : public UObject
{
	GENERATED_BODY()

public:
	static UDMMenuContext* CreateEmpty();
	static UDMMenuContext* CreateEditor(const TWeakPtr<SDMEditor>& InEditorWidget);
	static UDMMenuContext* CreateLayer(const TWeakPtr<SDMEditor>& InEditorWidget, UDMMaterialLayerObject* InLayerObject);
	static UDMMenuContext* CreateStage(const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget);

	static UToolMenu* GenerateContextMenuDefault(const FName InMenuName);
	static UToolMenu* GenerateContextMenuEditor(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget);
	static UToolMenu* GenerateContextMenuLayer(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget, UDMMaterialLayerObject* InLayerObject);
	static UToolMenu* GenerateContextMenuStage(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget);

	const TSharedPtr<SDMEditor> GetEditorWidget() const { return EditorWidgetWeak.Pin(); }
	const TSharedPtr<SDMStage> GetStageWidget() const { return StageWidgetWeak.Pin(); }

	UDMMaterialSlot* GetSlot() const;

	UDynamicMaterialModel* GetModel() const;

	UDMMaterialStage* GetStage() const;

	UDMMaterialStageSource* GetStageSource() const;

	UDMMaterialStageBlend* GetStageSourceAsBlend() const;

	const UDMMaterialLayerObject* GetLayer() const;

protected:
	TWeakPtr<SDMEditor> EditorWidgetWeak;
	TWeakPtr<SDMStage> StageWidgetWeak;
	TWeakObjectPtr<UDMMaterialLayerObject> LayerObjectWeak;

	static UDMMenuContext* Create(const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject);
	static UToolMenu* GenerateContextMenu(const FName InMenuName, const TWeakPtr<SDMEditor>& InEditorWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject);
};
