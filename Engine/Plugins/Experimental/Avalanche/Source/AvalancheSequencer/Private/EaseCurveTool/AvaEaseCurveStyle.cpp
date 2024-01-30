// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurveStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

FAvaEaseCurveStyle::FAvaEaseCurveStyle()
	: FSlateStyleSet(TEXT("AvaEaseCurveStyle"))
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));
	check(Plugin.IsValid());
	if (Plugin.IsValid())
	{
		SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
	}

	Set("EditMode.Background", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.f, FLinearColor(0.1f, 0.1f, 0.1f, 1.f), 1.f));
	Set("EditMode.Background.Highlight", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.f, FLinearColor(0.6f, 0.6f, 0.6f, 1.f), 1.f));
	Set("EditMode.Background.Over", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.f, FStyleColors::AccentBlue.GetSpecifiedColor(), 1.f));

	const FButtonStyle& SimpleButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>(TEXT("SimpleButton"));

	constexpr float ToolButtonPadding = 2.f;
	Set("ToolButton.Padding", ToolButtonPadding);

	constexpr float ToolButtonImageSize = 12.f;
	Set("ToolButton.ImageSize", ToolButtonImageSize);

	const FButtonStyle ToolButtonStyle = FButtonStyle(SimpleButtonStyle)
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetNormalPadding(FMargin(ToolButtonPadding))
		.SetPressedPadding(FMargin(ToolButtonPadding, ToolButtonPadding + (ToolButtonPadding * 0.5f), ToolButtonPadding, ToolButtonPadding - (ToolButtonPadding * 0.5f)));
	Set("ToolButton", ToolButtonStyle);

	const FButtonStyle ToolButtonNoPadStyle = FButtonStyle(ToolButtonStyle)
		.SetNormalPadding(FMargin(0.f))
		.SetPressedPadding(FMargin(0.f));
	Set("ToolButton.NoPad", ToolButtonNoPadStyle);

	const FCheckBoxStyle& ToggleButtonCheckboxStyle = FAppStyle::GetWidgetStyle<FCheckBoxStyle>(TEXT("ToggleButtonCheckbox"));

	const FCheckBoxStyle ToolToggleButtonStyle = FCheckBoxStyle(ToggleButtonCheckboxStyle)
		.SetUncheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetUncheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetCheckedHoveredImage(FSlateRoundedBoxBrush(FStyleColors::PrimaryHover, 4.0f))
		.SetCheckedPressedImage(FSlateRoundedBoxBrush(FStyleColors::PrimaryPress, 4.0f))
		.SetPadding(ToolButtonPadding);
	Set("ToolToggleButton", ToolToggleButtonStyle);

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaEaseCurveStyle::~FAvaEaseCurveStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
