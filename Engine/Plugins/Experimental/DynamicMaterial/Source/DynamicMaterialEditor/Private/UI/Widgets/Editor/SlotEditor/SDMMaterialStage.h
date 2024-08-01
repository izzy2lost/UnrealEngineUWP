// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class SDMMaterialEditor;
class SDMMaterialSlotLayerItem;
class UDMMaterialStage;
class UTexture;

class SDMMaterialStage : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SDMMaterialStage, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialStage) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMMaterialStage() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialSlotLayerItem>& InSlotLayerItem, UDMMaterialStage* InStage);

	TSharedPtr<SDMMaterialSlotLayerItem> GetSlotLayerView() const;

	UDMMaterialStage* GetStage() const;

	//~ Begin SWidget
	virtual FReply OnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	//~ End SWidget

protected:
	TWeakPtr<SDMMaterialSlotLayerItem> SlotLayerItemWeak;
	TWeakObjectPtr<UDMMaterialStage> StageWeak;

	bool IsStageSelected() const;

	const FSlateBrush* GetBorderBrush() const;

	/** Drag and drop. */
	bool OnAssetDraggedOver(TArrayView<FAssetData> InAssets);

	void OnAssetsDropped(const FDragDropEvent& InDragDropEvent, TArrayView<FAssetData> InAssets);

	void HandleDrop_Texture(UTexture* InTexture);
};
