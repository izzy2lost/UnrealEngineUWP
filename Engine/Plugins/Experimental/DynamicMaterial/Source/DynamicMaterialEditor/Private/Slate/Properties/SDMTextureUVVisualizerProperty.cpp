// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMTextureUVVisualizerProperty.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMTextureUV.h"
#include "DetailLayoutBuilder.h"
#include "DynamicMaterialEditorSettings.h"
#include "Slate/Properties/SDMTextureUVVisualizer.h"
#include "Slate/Properties/SDMTextureUVVisualizerPopout.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
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
			+ SHorizontalBox::Slot()
			.FillContentWidth(1.f)
			[
				SNew(SButton)
				.OnClicked(this, &SDMTextureUVVisualizerProperty::OnOpenPopoutClicked)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PopoutVisualizer", "Popout"))
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
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SDMTextureUVVisualizerProperty::GetVisualizerVisibility)
			+ SHorizontalBox::Slot()
			.FillContentWidth(1.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(FVector2D(5.f, 3.f))
				.IsChecked(this, &SDMTextureUVVisualizerProperty::GetModeCheckBoxState, /* Is Pivot */ false)
				.OnCheckStateChanged(this, &SDMTextureUVVisualizerProperty::OnModeCheckBoxStateChanged, /* Is Pivot */ false)
				.ToolTipText(LOCTEXT("VisualizerOffsetToolTip", "Allows changing of the UV offset."))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VisualizerOffset", "Offset"))
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
			]
			+ SHorizontalBox::Slot()
			.FillContentWidth(1.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.Padding(FVector2D(5.f, 3.f))
				.IsChecked(this, &SDMTextureUVVisualizerProperty::GetModeCheckBoxState, /* Is Pivot */ true)
				.OnCheckStateChanged(this, &SDMTextureUVVisualizerProperty::OnModeCheckBoxStateChanged, /* Is Pivot */ true)
				.ToolTipText(LOCTEXT("VisualizerPivotToolTip", "Allows changing of the UV pivot, rotation and scale."))
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("VisualizerPivot", "Pivot"))
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

FReply SDMTextureUVVisualizerProperty::OnOpenPopoutClicked()
{
	if (!Visualizer.IsValid())
	{
		return FReply::Handled();
	}

	UDMMaterialStage* Stage = Visualizer->GetStage();
	UDMTextureUV* TextureUV = Visualizer->GetTextureUV();

	if (!IsValid(Stage) || !IsValid(TextureUV))
	{
		return FReply::Handled();
	}

	SDMTextureUVVisualizerPopout::CreatePopout(Stage, TextureUV);

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

ECheckBoxState SDMTextureUVVisualizerProperty::GetModeCheckBoxState(bool bInIsPivot) const
{
	if (!Visualizer.IsValid())
	{
		return ECheckBoxState::Undetermined;
	}

	return Visualizer->IsInPivotEditMode() == bInIsPivot
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SDMTextureUVVisualizerProperty::OnModeCheckBoxStateChanged(ECheckBoxState InState, bool bInIsPivot)
{
	if (InState != ECheckBoxState::Checked || !Visualizer.IsValid())
	{
		return;
	}

	Visualizer->SetInPivotEditMode(bInIsPivot);
}

#undef LOCTEXT_NAMESPACE
