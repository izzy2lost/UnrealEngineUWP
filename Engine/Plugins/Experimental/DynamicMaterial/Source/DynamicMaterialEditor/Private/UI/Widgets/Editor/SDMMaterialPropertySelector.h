// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class SDMMaterialEditor;
class SDMMaterialSlotEditor;
class SWidget;
class UDMMaterialProperty;
class UDMMaterialSlot;
class UDynamicMaterialModelEditorOnlyData;
enum class ECheckBoxState : uint8;
enum class EDMMaterialPropertyType : uint8;

class SDMMaterialPropertySelector : public SCompoundWidget
{
	SLATE_DECLARE_WIDGET(SDMMaterialPropertySelector, SCompoundWidget)

	SLATE_BEGIN_ARGS(SDMMaterialPropertySelector) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMMaterialPropertySelector() override = default;

	void Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget);

	TSharedPtr<SDMMaterialEditor> GetEditorWidget() const;

	EDMMaterialPropertyType GetSelectedProperty() const;

	void SetGlobalSettings();

	void SetSelectedProperty(EDMMaterialPropertyType InMaterialProperty);

protected:
	TWeakPtr<SDMMaterialEditor> EditorWidgetWeak;
	EDMMaterialPropertyType SelectedProperty;

	UDynamicMaterialModelEditorOnlyData* GetEditorOnlyData() const;

	TSharedPtr<SDMMaterialSlotEditor> GetSlotEditorWidget() const;

	TSharedRef<SWidget> CreateSlot_PropertyList();

	TSharedRef<SWidget> CreateSlot_EnabledButton(EDMMaterialPropertyType InMaterialProperty);

	TSharedRef<SWidget> CreateSlot_SelectButton(EDMMaterialPropertyType InMaterialProperty);

	bool IsPropertyEnabled(EDMMaterialPropertyType InMaterialProperty) const;

	bool SetPropertyEnabled(EDMMaterialPropertyType InMaterialProperty, bool bInEnabled);

	bool DoesPropertySlotExist(EDMMaterialPropertyType InMaterialProperty) const;

	bool GetPropertyEnabledEnabled(EDMMaterialPropertyType InMaterialProperty) const;
	ECheckBoxState GetPropertyEnabledState(EDMMaterialPropertyType InMaterialProperty) const;
	void OnPropertyEnabledStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty);

	bool GetPropertySelectEnabled(EDMMaterialPropertyType InMaterialProperty) const;
	ECheckBoxState GetPropertySelectState(EDMMaterialPropertyType InMaterialProperty) const;
	void OnPropertySelectStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty);
	FSlateColor GetPropertySelectButtonChipColor(EDMMaterialPropertyType InMaterialProperty) const;

	void OnSelectedPropertyChanged();
};
