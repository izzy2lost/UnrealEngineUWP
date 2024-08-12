// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Widgets/Editor/PropertySelectorLayouts/SDMMaterialPropertySelector_VerticalSlim.h"

#include "DetailLayoutBuilder.h"
#include "DMDefs.h"
#include "Utils/DMPrivate.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMMaterialPropertySelector_VerticalSlim"

void SDMMaterialPropertySelector_VerticalSlim::Construct(const FArguments& InArgs, const TSharedRef<SDMMaterialEditor> InEditorWidget)
{
	SDMMaterialPropertySelector_VerticalBase::Construct(
		SDMMaterialPropertySelector_VerticalBase::FArguments(),
		InEditorWidget
	);
}

TSharedRef<SWidget> SDMMaterialPropertySelector_VerticalSlim::CreateSlot_SelectButton(EDMMaterialPropertyType InMaterialProperty)
{
	const FText ButtonText = GetSelectButtonText(InMaterialProperty, /* Short Name */ true);
	const FText ToolTip = GetButtonToolTip(InMaterialProperty);

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.Padding(0.f)
		.IsEnabled(this, &SDMMaterialPropertySelector_VerticalSlim::GetPropertySelectEnabled, InMaterialProperty)
		.IsChecked(this, &SDMMaterialPropertySelector_VerticalSlim::GetPropertySelectState, InMaterialProperty)
		.OnCheckStateChanged(this, &SDMMaterialPropertySelector_VerticalSlim::OnPropertySelectStateChanged, InMaterialProperty)
		.ToolTipText(ToolTip)
		.Content()
		[
			SNew(SBox)
			.WidthOverride(42.f)
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("FilterBar.FilterImage"))
					.ColorAndOpacity(this, &SDMMaterialPropertySelector_VerticalSlim::GetPropertySelectButtonChipColor, InMaterialProperty)
				]
				+SHorizontalBox::Slot()
				.Padding(2.f, 6.f)
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

#undef LOCTEXT_NAMESPACE
