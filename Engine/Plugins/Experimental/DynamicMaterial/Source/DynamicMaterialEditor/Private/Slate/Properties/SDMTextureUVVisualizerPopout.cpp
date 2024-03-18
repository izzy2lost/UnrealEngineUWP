// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Properties/SDMTextureUVVisualizerPopout.h"
#include "DetailLayoutBuilder.h"
#include "Slate/Properties/SDMTextureUVVisualizer.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMTextureUVVisualizerPopout"

void SDMTextureUVVisualizerPopout::Construct(const FArguments& InArgs, UDMMaterialStage* InMaterialStage, UDMTextureUV* InTextureUV)
{
	check(InMaterialStage);
	check(InTextureUV);

	SWindow::Construct(
		SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Material Designer Texture UV Visualizer"))
		.ClientSize(FVector2f(1024, 768))
		.MinWidth(128)
		.MinHeight(160)
	);

	SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		[
			SNew(SButton)
			.OnClicked(this, &SDMTextureUVVisualizerPopout::OnToggleModeClicked)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SDMTextureUVVisualizerPopout::GetModeButtonText)
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		+ SVerticalBox::Slot()
		.VAlign(EVerticalAlignment::VAlign_Fill)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		.Padding(0.f, 3.f, 0.f, 0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(Visualizer, SDMTextureUVVisualizer, InMaterialStage, InTextureUV)
				.IsPopout(true)
			]
			+ SOverlay::Slot()
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				.VAlign(EVerticalAlignment::VAlign_Top)
				[
					SNew(SColorBlock)
					.Color(FLinearColor(0, 0, 0, 0.5))
					.Size(this, &SDMTextureUVVisualizerPopout::GetHorizontalBarSize)
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				.VAlign(EVerticalAlignment::VAlign_Bottom)
				[
					SNew(SColorBlock)
					.Color(FLinearColor(0, 0, 0, 0.5))
					.Size(this, &SDMTextureUVVisualizerPopout::GetHorizontalBarSize)
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Left)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SColorBlock)
					.Color(FLinearColor(0, 0, 0, 0.5))
					.Size(this, &SDMTextureUVVisualizerPopout::GetSideBlockSize)
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SBox)
				.HAlign(EHorizontalAlignment::HAlign_Right)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SColorBlock)
					.Color(FLinearColor(0, 0, 0, 0.5))
					.Size(this, &SDMTextureUVVisualizerPopout::GetSideBlockSize)
				]
			]
		]
	);
}

FReply SDMTextureUVVisualizerPopout::OnToggleModeClicked()
{
	if (Visualizer.IsValid())
	{
		Visualizer->TogglePivotEditMode();
	}

	return FReply::Handled();
}

FText SDMTextureUVVisualizerPopout::GetModeButtonText() const
{
	if (Visualizer.IsValid() && Visualizer->IsInPivotEditMode())
	{
		return LOCTEXT("VisualizerPivot", "Pivot");
	}

	return LOCTEXT("VisualizerOffset", "Offset");
}

FVector2D SDMTextureUVVisualizerPopout::GetHorizontalBarSize() const
{
	if (Visualizer.IsValid())
	{
		const FVector2f LocalSize = Visualizer->GetTickSpaceGeometry().GetLocalSize();

		if (!FMath::IsNearlyZero(LocalSize.X) && !FMath::IsNearlyZero(LocalSize.Y))
		{
			if (LocalSize.Y <= LocalSize.X)
			{
				return FVector2D(1, LocalSize.Y / 3.f);
			}

			return FVector2D(1, LocalSize.X / 3.f + (LocalSize.Y - LocalSize.X) * 0.5f);
		}
	}

	return FVector2D::UnitVector;
}

FVector2D SDMTextureUVVisualizerPopout::GetSideBlockSize() const
{
	if (Visualizer.IsValid())
	{
		const FVector2f LocalSize = Visualizer->GetTickSpaceGeometry().GetLocalSize();

		if (!FMath::IsNearlyZero(LocalSize.X) && !FMath::IsNearlyZero(LocalSize.Y))
		{
			if (LocalSize.X <= LocalSize.Y)
			{
				return FVector2D(LocalSize.X / 3.f, LocalSize.X / 3.f);
			}

			return FVector2D(LocalSize.Y / 3.f + (LocalSize.X - LocalSize.Y) * 0.5f, LocalSize.Y / 3.f);
		}
	}

	return FVector2D::UnitVector;
}

#undef LOCTEXT_NAMESPACE
