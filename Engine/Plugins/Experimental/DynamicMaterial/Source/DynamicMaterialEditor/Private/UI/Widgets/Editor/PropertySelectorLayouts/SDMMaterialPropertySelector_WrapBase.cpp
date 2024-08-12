// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/PropertySelectorLayouts/SDMMaterialPropertySelector_WrapBase.h"

#include "Components/DMMaterialProperty.h"
#include "DetailLayoutBuilder.h"
#include "DMDefs.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertySelector_WrapBase"

void SDMMaterialPropertySelector_WrapBase::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget)
{
	SDMMaterialPropertySelector::Construct(
		SDMMaterialPropertySelector::FArguments(),
		InEditorWidget
	);
}

TSharedRef<SWidget> SDMMaterialPropertySelector_WrapBase::CreateSlot_PropertyList()
{
	using namespace UE::DynamicMaterialEditor::Private;

	TSharedRef<SWrapBox> NewSlotList = SNew(SWrapBox)
		.InnerSlotPadding(FVector2D(6.f, 3.f))
		.UseAllottedSize(true);

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = GetEditorOnlyData();

	if (!EditorOnlyData)
	{
		return NewSlotList;
	}

	NewSlotList->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(20.f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				CreateSlot_SelectButton(EDMMaterialPropertyType::None)
			]
		];

	NewSlotList->AddSlot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(20.f)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				CreateSlot_SelectButton(EDMMaterialPropertyType::Any)
			]
		];

	for (const TPair<EDMMaterialPropertyType, UDMMaterialProperty*>& PropertyPair : EditorOnlyData->GetMaterialProperties())
	{
		if (IsCustomMaterialProperty(PropertyPair.Key))
		{
			continue;
		}

		NewSlotList->AddSlot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateSlot_EnabledButton(PropertyPair.Key)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CreateSlot_SelectButton(PropertyPair.Key)
				]
			];
	}

	return NewSlotList;
}

TSharedRef<SWidget> SDMMaterialPropertySelector_WrapBase::CreateSlot_SelectButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FText ButtonText = GetSelectButtonText(InMaterialProperty, /* Short Name */ true);
	const FText ToolTip = GetButtonToolTip(InMaterialProperty);

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.Padding(0.f)
		.IsEnabled(this, &SDMMaterialPropertySelector_WrapBase::GetPropertySelectEnabled, InMaterialProperty)
		.IsChecked(this, &SDMMaterialPropertySelector_WrapBase::GetPropertySelectState, InMaterialProperty)
		.OnCheckStateChanged(this, &SDMMaterialPropertySelector_WrapBase::OnPropertySelectStateChanged, InMaterialProperty)
		.ToolTipText(ToolTip)
		.Content()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("FilterBar.FilterImage"))
				.ColorAndOpacity(this, &SDMMaterialPropertySelector_WrapBase::GetPropertySelectButtonChipColor, InMaterialProperty)
			]
			+SHorizontalBox::Slot()
			.Padding(4.f, 6.f)
			.VAlign(VAlign_Center)
			.FillWidth(1.f)
			[
				SNew(SBox)
				.WidthOverride(32.f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(ButtonText)
					.Justification(ETextJustify::Center)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
