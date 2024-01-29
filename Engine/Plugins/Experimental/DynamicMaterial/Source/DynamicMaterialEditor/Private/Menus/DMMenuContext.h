// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "DMMenuContext.generated.h"

class SDMSlot;
class SDMStage;
class UDMMaterialLayerObject;
class UDMMaterialSlot;
class UDMMaterialStage;
class UDMMaterialStageBlend;
class UDMMaterialStageSource;
class UDynamicMaterialModel;
class UToolMenu;

UCLASS(BlueprintType)
class DYNAMICMATERIALEDITOR_API UDMMenuContext : public UObject
{
	GENERATED_BODY()

public:
	static UDMMenuContext* CreateEmpty();
	static UDMMenuContext* CreateSlot(const TWeakPtr<SDMSlot>& InSlotWidget);
	static UDMMenuContext* CreateLayer(const TWeakPtr<SDMSlot>& InSlotWidget, UDMMaterialLayerObject* InLayerObject);
	static UDMMenuContext* CreateStage(const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget);

	static UToolMenu* GenerateContextMenuDefault(const FName InMenuName);
	static UToolMenu* GenerateContextMenuSlot(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget);
	static UToolMenu* GenerateContextMenuLayer(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget, UDMMaterialLayerObject* InLayerObject);
	static UToolMenu* GenerateContextMenuStage(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget);

	const TWeakPtr<SDMSlot>& GetSlotWidget() const { return SlotWidgetWeak; }
	const TWeakPtr<SDMStage>& GetStageWidget() const { return StageWidgetWeak; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer Extensions")
	UDMMaterialSlot* GetSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer Extensions")
	UDynamicMaterialModel* GetModel() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer Extensions")
	UDMMaterialStage* GetStage() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer Extensions")
	UDMMaterialStageSource* GetStageSource() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer Extensions")
	UDMMaterialStageBlend* GetStageSourceAsBlend() const;

	const UDMMaterialLayerObject* GetLayer() const;

protected:
	TWeakPtr<SDMSlot> SlotWidgetWeak;
	TWeakPtr<SDMStage> StageWidgetWeak;
	TWeakObjectPtr<UDMMaterialLayerObject> LayerObjectWeak;

	static UDMMenuContext* Create(const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject);
	static UToolMenu* GenerateContextMenu(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject);
};
