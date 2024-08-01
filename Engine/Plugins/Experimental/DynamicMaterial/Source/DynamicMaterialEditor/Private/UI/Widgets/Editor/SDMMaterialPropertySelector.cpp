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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertySelector"

void SDMMaterialPropertySelector::PrivateRegisterAttributes(FSlateAttributeDescriptor::FInitializer&)
{
}

void SDMMaterialPropertySelector::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget)
{
	EditorWidgetWeak = InEditorWidget;
	SelectedProperty = EDMMaterialPropertyType::None;

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

	TSharedRef<SVerticalBox> NewSlotList = SNew(SVerticalBox);

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return NewSlotList;
	}

	NewSlotList->AddSlot()
		.AutoHeight()
		[
			CreateSlot_SelectButton(EDMMaterialPropertyType::None)
		];

	for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
	{
		if (IsCustomMaterialProperty(PropertyPair.Key))
		{
			continue;
		}

		NewSlotList->AddSlot()
			.AutoHeight()
			[
				CreateSlot_SelectButton(PropertyPair.Key)
			];
	}

	return NewSlotList;
}

TSharedRef<SWidget> SDMMaterialPropertySelector::CreateSlot_SelectButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FText ButtonText = InMaterialProperty == EDMMaterialPropertyType::None
		? LOCTEXT("GlobalSettings", "Global Settings")
		: StaticEnum<EDMMaterialPropertyType>()->GetDisplayNameTextByValue(static_cast<int64>(InMaterialProperty));

	const FText Format = LOCTEXT("PropertyFormat", "Edit the {0} property.\n\n- Control+Left click: Toggle the property\n\nProperty may not be toggleable if it is not valid for the material type..");
	UEnum* MaterialPropertyEnum = StaticEnum<EDMMaterialPropertyType>();

	const FText ToolTip = (InMaterialProperty == EDMMaterialPropertyType::None)
		? LOCTEXT("GeneralSettingsToolTip", "Edit the material global settings.")
		: FText::Format(Format, MaterialPropertyEnum->GetDisplayNameTextByValue(static_cast<int64>(InMaterialProperty)));

	TSharedRef<SBox> Outer = SNew(SBox)
		[		
			SNew(SCheckBox)
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
			]
		];

	// This is triggered when the inner checkbox is disabled.
	Outer->SetOnMouseButtonDown(FPointerEventHandler::CreateSP(this, &SDMMaterialPropertySelector::OnPropertySelectMouseDown, InMaterialProperty));

	return Outer;
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


	const FSlateApplication& SlateApplication = FSlateApplication::Get();

	if (SlateApplication.GetModifierKeys().IsControlDown() || SlateApplication.GetModifierKeys().IsCommandDown())
	{
		SetPropertyEnabled(InMaterialProperty, /* Enabled */ false);
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

FReply SDMMaterialPropertySelector::OnPropertySelectMouseDown(const FGeometry& InGeometry, const FPointerEvent& InEvent, 
	EDMMaterialPropertyType InMaterialProperty)
{
	if (!InEvent.GetModifierKeys().IsControlDown() && !InEvent.GetModifierKeys().IsCommandDown())
	{
		return FReply::Unhandled();
	}

	if (SetPropertyEnabled(InMaterialProperty, /* Enabled */ true))
	{
		SetSelectedProperty(InMaterialProperty);
	}

	return FReply::Handled();
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
