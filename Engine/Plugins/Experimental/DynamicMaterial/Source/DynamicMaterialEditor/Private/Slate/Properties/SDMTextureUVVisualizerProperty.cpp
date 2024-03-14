// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMTextureUVVisualizerProperty.h"
#include "DetailLayoutBuilder.h"
#include "DynamicMaterialEditorSettings.h"
#include "Slate/Properties/SDMTextureUVVisualizer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMTextureUVVisualizerProperty"

void SDMTextureUVVisualizerProperty::Construct(const FArguments& InArgs, UDMMaterialStage* InMaterialStage, UDMTextureUV* InTextureUV)
{
	check(InMaterialStage);
	check(InTextureUV);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SDMTextureUVVisualizerProperty::GetVisualizerVisibility)
			+ SHorizontalBox::Slot()
			.FillContentWidth(1.f)
			[
				SNew(SButton)
				.OnClicked(this, &SDMTextureUVVisualizerProperty::OnOpenPopoutClicked)
				.IsEnabled(false)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PopoutVisualizer", "Popout"))
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
			]
			+ SHorizontalBox::Slot()
			.FillContentWidth(1.f)
			[
				SNew(SButton)
				.OnClicked(this, &SDMTextureUVVisualizerProperty::OnToggleModeClicked)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &SDMTextureUVVisualizerProperty::GetModeButtonText)
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Top)
		.Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(SBox)
			.Visibility(this, &SDMTextureUVVisualizerProperty::GetVisualizerVisibility)
			.MinAspectRatio(1)
			.MaxAspectRatio(1)
			[
				SAssignNew(Visualizer, SDMTextureUVVisualizer, InMaterialStage, InTextureUV)
					.IsPopout(false)
			]			
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.OnClicked(this, &SDMTextureUVVisualizerProperty::OnToggleVisualizerClicked)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ToggleVisualizer", "Toggle"))
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
			]
		]
	];
}


FReply SDMTextureUVVisualizerProperty::OnToggleVisualizerClicked()
{
	if (UDynamicMaterialEditorSettings* Settings = GetMutableDefault<UDynamicMaterialEditorSettings>())
	{
		Settings->bUVVisualizerVisible = !Settings->bUVVisualizerVisible;
		Settings->SaveConfig();
	}

	return FReply::Handled();
}

FReply SDMTextureUVVisualizerProperty::OnToggleModeClicked()
{
	if (Visualizer.IsValid())
	{
		Visualizer->TogglePivotEditMode();
	}

	return FReply::Handled();
}

FText SDMTextureUVVisualizerProperty::GetModeButtonText() const
{
	if (Visualizer.IsValid() && Visualizer->IsInPivotEditMode())
	{
		return LOCTEXT("VisualizerPivot", "Pivot");
	}

	return LOCTEXT("VisualizerOffset", "Offset");
}

FReply SDMTextureUVVisualizerProperty::OnOpenPopoutClicked()
{
	return FReply::Handled();
}

EVisibility SDMTextureUVVisualizerProperty::GetVisualizerVisibility() const
{
	if (const UDynamicMaterialEditorSettings* Settings = GetDefault<UDynamicMaterialEditorSettings>())
	{
		return Settings->bUVVisualizerVisible
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
