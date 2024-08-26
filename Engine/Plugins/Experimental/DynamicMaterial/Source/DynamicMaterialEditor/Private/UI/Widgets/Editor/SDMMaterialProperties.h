// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Containers/Array.h"
#include "UI/Utils/DMWidgetSlot.h"

class ICustomDetailsViewItem;
class SDMMaterialEditor;
class SVerticalBox;
class UDMMaterialProperty;
class UDMMaterialSlot;
enum class ECheckBoxState : uint8;
enum class EDMMaterialPropertyType : uint8;

class SDMMaterialProperties : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SDMMaterialProperties, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialProperties) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMMaterialProperties() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor>& InEditorWidget);

	void Validate();

protected:
	TWeakPtr<SDMMaterialEditor> EditorWidgetWeak;

	TDMWidgetSlot<SWidget> Content;

	TArray<TSharedRef<ICustomDetailsViewItem>> GlobalItems;

	TSharedRef<SWidget> CreateSlot_Content();

	void AddProperty(const TSharedRef<SVerticalBox>& InList, UDMMaterialProperty* InProperty);

	TSharedRef<SWidget> CreateSlot_EnabledButton(EDMMaterialPropertyType InMaterialProperty);

	TSharedRef<SWidget> CreateSlot_PropertyName(EDMMaterialPropertyType InMaterialProperty);

	bool GetPropertyEnabledEnabled(EDMMaterialPropertyType InMaterialProperty) const;
	ECheckBoxState GetPropertyEnabledState(EDMMaterialPropertyType InMaterialProperty) const;
	void OnPropertyEnabledStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty);

	FReply OnPropertyClicked(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, EDMMaterialPropertyType InMaterialProperty);

	TSharedRef<SWidget> CreateGlobalSlider(UDMMaterialProperty* InProperty);
};
