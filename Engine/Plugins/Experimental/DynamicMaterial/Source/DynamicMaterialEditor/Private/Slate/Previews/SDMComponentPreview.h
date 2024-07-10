// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "SlateMaterialBrush.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class SDMEditor;
class UDMMaterialComponent;
class UDMMaterialValueDynamic;
class UDMTextureUV;
class UDMTextureUVDynamic;
class UDynamicMaterialModelBase;
class UDynamicMaterialModelDynamic;

class SDMComponentPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMComponentPreview)
		: _PreviewSize(FVector2D(48.f, 48.f))
		{}
		SLATE_ATTRIBUTE(FVector2D, PreviewSize)
	SLATE_END_ARGS()

	SDMComponentPreview();

	virtual ~SDMComponentPreview() override;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditorWidget, UDMMaterialComponent* InComponent);

	FSlateMaterialBrush& GetBrush() { return Brush; }

	//~ Begin SWidget
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

protected:
	TWeakPtr<SDMEditor> EditorWidgetWeak;
	TWeakObjectPtr<UDMMaterialComponent> ComponentWeak;
	TWeakObjectPtr<UDynamicMaterialModelBase> MaterialModelBaseWeak;
	TWeakObjectPtr<UMaterial> PreviewMaterialBaseWeak;
	TWeakObjectPtr<UMaterialInstanceDynamic> PreviewMaterialDynamicWeak;
	FSlateMaterialBrush Brush;
	TAttribute<FVector2D> PreviewSize;

	void OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	TOptional<FVector2D> GetPreviewSize() const;

	void OnValueUpdated(UDynamicMaterialModel* InMaterialModel, UDMMaterialValue* InValue);

	void OnTextureUVUpdated(UDynamicMaterialModel* InMaterialModel, UDMTextureUV* InTextureUV);

	void OnValueDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMMaterialValueDynamic* InValueDynamic);

	void OnTextureUVDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMTextureUVDynamic* InTextureUVDynamic);
};
