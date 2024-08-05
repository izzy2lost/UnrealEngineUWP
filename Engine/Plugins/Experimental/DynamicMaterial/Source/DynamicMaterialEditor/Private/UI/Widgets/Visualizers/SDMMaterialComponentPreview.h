// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "DMEDefs.h"
#include "SlateMaterialBrush.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class SDMMaterialEditor;
class UDMMaterialComponent;
class UDMMaterialValueDynamic;
class UDMTextureUV;
class UDMTextureUVDynamic;
class UDynamicMaterialModelBase;
class UDynamicMaterialModelDynamic;

class SDMMaterialComponentPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMMaterialComponentPreview)
		: _PreviewSize(FVector2D(48.f, 48.f))
		{}
		SLATE_ATTRIBUTE(TOptional<FVector2D>, PreviewSize)
	SLATE_END_ARGS()

	SDMMaterialComponentPreview();

	virtual ~SDMMaterialComponentPreview() override;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget, UDMMaterialComponent* InComponent);

	FSlateMaterialBrush& GetBrush() { return Brush; }

	//~ Begin SWidget
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

protected:
	TWeakPtr<SDMMaterialEditor> EditorWidgetWeak;
	TWeakObjectPtr<UDMMaterialComponent> ComponentWeak;
	TWeakObjectPtr<UDynamicMaterialModelBase> MaterialModelBaseWeak;
	TWeakObjectPtr<UMaterial> PreviewMaterialBaseWeak;
	TWeakObjectPtr<UMaterialInstanceDynamic> PreviewMaterialDynamicWeak;
	FSlateMaterialBrush Brush;

	void OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	void OnValueUpdated(UDynamicMaterialModel* InMaterialModel, UDMMaterialValue* InValue);

	void OnTextureUVUpdated(UDynamicMaterialModel* InMaterialModel, UDMTextureUV* InTextureUV);

	void OnValueDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMMaterialValueDynamic* InValueDynamic);

	void OnTextureUVDynamicUpdated(UDynamicMaterialModelDynamic* InMaterialModel, UDMTextureUVDynamic* InTextureUVDynamic);
};
