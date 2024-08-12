// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "UI/Utils/DMWidgetSlot.h"

class SDMMaterialEditor;
class SWrapBox;
class UDMMaterialProperty;
class UDMMaterialSlot;
enum class ECheckBoxState : uint8;
enum class EDMMaterialPropertyType : uint8;

class SDMMaterialPropertyPreviews : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SDMMaterialPropertyPreviews, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialPropertyPreviews) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMMaterialPropertyPreviews() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget);

	void Validate();

protected:
	TWeakPtr<SDMMaterialEditor> EditorWidgetWeak;

	TDMWidgetSlot<SWidget> Content;

	TSharedRef<SWidget> CreateSlot_Content();

	void AddPropertyPreview(const TSharedRef<SWrapBox>& InContainer, EDMMaterialPropertyType InMaterialProperty, UDMMaterialSlot* InSlot);

	TSharedRef<SWidget> CreateSlot_EnabledButton(EDMMaterialPropertyType InMaterialProperty);

	TSharedRef<SWidget> CreateSlot_PropertyName(EDMMaterialPropertyType InMaterialProperty);

	bool GetPropertyEnabledEnabled(EDMMaterialPropertyType InMaterialProperty) const;
	ECheckBoxState GetPropertyEnabledState(EDMMaterialPropertyType InMaterialProperty) const;
	void OnPropertyEnabledStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty);
};
