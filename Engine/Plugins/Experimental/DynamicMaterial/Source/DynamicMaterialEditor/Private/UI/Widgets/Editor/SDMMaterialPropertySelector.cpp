// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/SDMMaterialPropertySelector.h"

#include "Components/DMMaterialProperty.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Styling/StyleColors.h"
#include "UI/Widgets/SDMMaterialEditor.h"
#include "UObject/Class.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertySelector"

namespace UE::DynamicMaterialEditor::Private
{
	namespace PropertySelectorColumns
	{
		constexpr int32 Enable = 0;
		constexpr int32 Select = 1;
	}
}

void SDMMaterialPropertySelector::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SDMMaterialPropertySelector::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget)
{
	EditorWidgetWeak = InEditorWidget;
	SelectedProperty = EDMMaterialPropertyType::None;

	SetCanTick(false);

	ChildSlot
	[
		SNew(SScrollBox)
		.Orientation(EOrientation::Orient_Vertical)
		+ SScrollBox::Slot()
		[
			CreateSlot_PropertyList()
		]
	];
}

TSharedPtr<SDMMaterialEditor> SDMMaterialPropertySelector::GetEditorWidget() const
{
	return EditorWidgetWeak.Pin();
}

EDMMaterialPropertyType SDMMaterialPropertySelector::GetSelectedProperty() const
{
	return SelectedProperty;
}

void SDMMaterialPropertySelector::SetGlobalSettings()
{
	SelectedProperty = EDMMaterialPropertyType::None;

	TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	EditorWidget->EditSlot(nullptr);
}

void SDMMaterialPropertySelector::SetSelectedProperty(EDMMaterialPropertyType InMaterialProperty)
{
	if (SelectedProperty == InMaterialProperty)
	{
		return;
	}

	SelectedProperty = InMaterialProperty;

	OnSelectedPropertyChanged();
}

UDynamicMaterialModelEditorOnlyData* SDMMaterialPropertySelector::GetEditorOnlyData() const
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget();

	if (!EditorWidget.IsValid())
	{
		return nullptr;
	}

	UDynamicMaterialModel* MaterialModel = EditorWidget->GetMaterialModel();

	if (!IsValid(MaterialModel))
	{
		return nullptr;
	}

	return UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
}

TSharedPtr<SDMMaterialSlotEditor> SDMMaterialPropertySelector::GetSlotEditorWidget() const
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget();

	if (!EditorWidget.IsValid())
	{
		return nullptr;
	}

	return EditorWidget->GetSlotEditorWidget();
}

TSharedRef<SWidget> SDMMaterialPropertySelector::CreateSlot_PropertyList()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SGridPanel> NewSlotList = SNew(SGridPanel)
		.FillColumn(PropertySelectorColumns::Select, 1.f);

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return NewSlotList;
	}

	int32 Row = 0;

	NewSlotList->AddSlot(PropertySelectorColumns::Select, Row)
		[
			CreateSlot_SelectButton(EDMMaterialPropertyType::None)
		];

	++Row;

	for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
	{
		if (IsCustomMaterialProperty(PropertyPair.Key))
		{
			continue;
		}

		NewSlotList->AddSlot(PropertySelectorColumns::Enable, Row)
			[
				CreateSlot_EnabledButton(PropertyPair.Key)
			];

		NewSlotList->AddSlot(PropertySelectorColumns::Select, Row)
			[
				CreateSlot_SelectButton(PropertyPair.Key)
			];

		++Row;
	}

	return NewSlotList;
}

TSharedRef<SWidget> SDMMaterialPropertySelector::CreateSlot_EnabledButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FText Format = LOCTEXT("PropertyEnableFormat", "Toggle the {0} property.\n\nProperty must be valid for the Material Type.");
	UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

	const FText ToolTip = FText::Format(Format, MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(InMaterialProperty)));

	return SNew(SCheckBox)
		.IsEnabled(this, &SDMMaterialPropertySelector::GetPropertyEnabledEnabled, InMaterialProperty)
		.IsChecked(this, &SDMMaterialPropertySelector::GetPropertyEnabledState, InMaterialProperty)
		.OnCheckStateChanged(this, &SDMMaterialPropertySelector::OnPropertyEnabledStateChanged, InMaterialProperty)
		.ToolTipText(ToolTip);
}

TSharedRef<SWidget> SDMMaterialPropertySelector::CreateSlot_SelectButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FText ButtonText = InMaterialProperty == EDMMaterialPropertyType::None
		? LOCTEXT("GlobalSettings", "Global Settings")
		: StaticEnum<EDMMaterialPropertyType>()->GetDisplayNameTextByValue(static_cast<int64>(InMaterialProperty));

	const FText Format = LOCTEXT("PropertySelectFormat", "Edit the {0} property.");
	UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

	const FText ToolTip = (InMaterialProperty == EDMMaterialPropertyType::None)
		? LOCTEXT("GeneralSettingsToolTip", "Edit the material global settings.")
		: FText::Format(Format, MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(InMaterialProperty)));

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.Padding(0.f)
		.IsEnabled(this, &SDMMaterialPropertySelector::GetPropertySelectEnabled, InMaterialProperty)
		.IsChecked(this, &SDMMaterialPropertySelector::GetPropertySelectState, InMaterialProperty)
		.OnCheckStateChanged(this, &SDMMaterialPropertySelector::OnPropertySelectStateChanged, InMaterialProperty)
		.ToolTipText(ToolTip)
		.Content()
		[
			SNew(SBox)
			.WidthOverride(135.f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("FilterBar.FilterImage"))
					.ColorAndOpacity(this, &SDMMaterialPropertySelector::GetPropertySelectButtonChipColor, InMaterialProperty)
				]
				+SHorizontalBox::Slot()
				.Padding(10.f, 6.f)
				.VAlign(VAlign_Center)
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(ButtonText)
				]
			]
		];
}

bool SDMMaterialPropertySelector::IsPropertyEnabled(EDMMaterialPropertyType InMaterialProperty) const
{
	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return false;
	}

	return !!EditorOnlyData->GetMaterialProperty(InMaterialProperty);
}

bool SDMMaterialPropertySelector::DoesPropertySlotExist(EDMMaterialPropertyType InMaterialProperty) const
{
	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return false;
	}

	UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!MaterialProperty)
	{
		return false;
	}

	if (!MaterialProperty->IsEnabled())
	{
		return false;
	}

	return !!EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty);
}

bool SDMMaterialPropertySelector::SetPropertyEnabled(EDMMaterialPropertyType InMaterialProperty, bool bInEnabled)
{
	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return false;
	}

	UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!MaterialProperty)
	{
		return false;
	}

	MaterialProperty->SetEnabled(bInEnabled);

	if (InMaterialProperty == SelectedProperty)
	{
		SetGlobalSettings();
	}

	if (!bInEnabled)
	{
		return true;
	}

	if (EditorOnlyData->GetSlotForMaterialProperty(InMaterialProperty))
	{
		return true;
	}

	// Return true if the slot was successfully added
	return !!EditorOnlyData->AddSlotForMaterialProperty(InMaterialProperty);
}

bool SDMMaterialPropertySelector::GetPropertyEnabledEnabled(EDMMaterialPropertyType InMaterialProperty) const
{
	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return false;
	}

	UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!MaterialProperty)
	{
		return false;
	}

	return MaterialProperty->IsValidForModel(*EditorOnlyData);
}

ECheckBoxState SDMMaterialPropertySelector::GetPropertyEnabledState(EDMMaterialPropertyType InMaterialProperty) const
{
	return DoesPropertySlotExist(InMaterialProperty)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SDMMaterialPropertySelector::OnPropertyEnabledStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty)
{
	const bool bSetEnabled = InState == ECheckBoxState::Checked;

	if (SetPropertyEnabled(InMaterialProperty, /* Enabled */ bSetEnabled))
	{
		if (bSetEnabled)
		{
			SetSelectedProperty(InMaterialProperty);
		}
	}
}

bool SDMMaterialPropertySelector::GetPropertySelectEnabled(EDMMaterialPropertyType InMaterialProperty) const
{
	if (InMaterialProperty == EDMMaterialPropertyType::None)
	{
		return true;
	}

	return DoesPropertySlotExist(InMaterialProperty);
}

ECheckBoxState SDMMaterialPropertySelector::GetPropertySelectState(EDMMaterialPropertyType InMaterialProperty) const
{
	return InMaterialProperty == SelectedProperty
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SDMMaterialPropertySelector::OnPropertySelectStateChanged(ECheckBoxState InState, EDMMaterialPropertyType InMaterialProperty)
{
	if (InMaterialProperty == EDMMaterialPropertyType::None)
	{
		SetGlobalSettings();
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return;
	}

	UDMMaterialProperty* MaterialProperty = EditorOnlyData->GetMaterialProperty(InMaterialProperty);

	if (!MaterialProperty)
	{
		return;
	}

	if (!MaterialProperty->IsEnabled())
	{
		return;
	}

	SetSelectedProperty(InMaterialProperty);
}

FSlateColor SDMMaterialPropertySelector::GetPropertySelectButtonChipColor(EDMMaterialPropertyType InMaterialProperty) const
{
	if (InMaterialProperty == EDMMaterialPropertyType::None)
	{
		return FStyleColors::AccentGreen;
	}

	if (GetPropertySelectEnabled(InMaterialProperty))
	{
		return FStyleColors::Primary;
	}

	return FStyleColors::Panel;
}

void SDMMaterialPropertySelector::OnSelectedPropertyChanged()
{
	TSharedPtr<SDMMaterialEditor> EditorWidget = GetEditorWidget();

	if (!EditorWidget.IsValid())
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return;
	}

	UDMMaterialSlot* Slot = EditorOnlyData->GetSlotForMaterialProperty(SelectedProperty);

	if (!Slot)
	{
		Slot = EditorOnlyData->AddSlotForMaterialProperty(SelectedProperty);

		if (!Slot)
		{
			return;
		}
	}

	EditorWidget->EditSlot(Slot);
}

#undef LOCTEXT_NAMESPACE
