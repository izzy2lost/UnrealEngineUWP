// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedRenamerStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"

TSharedPtr<FSlateStyleSet> FAdvancedRenamerStyle::StyleInstance = nullptr;

void FAdvancedRenamerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		InitStyle();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAdvancedRenamerStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

FName FAdvancedRenamerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AdvancedRenamerStyle"));
	return StyleSetName;
}

void FAdvancedRenamerStyle::InitStyle()
{
	if (StyleInstance.IsValid())
	{
		return;
	}

	StyleInstance = MakeShared<FSlateStyleSet>("AdvancedRenamer");

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("AdvancedRenamer"));
	check(Plugin.IsValid());

	if (Plugin.IsValid())
	{
		StyleInstance->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
	}

	const FSplitterStyle SplitterStyle = FSplitterStyle()
		.SetHandleNormalBrush(FSlateNoResource())
		.SetHandleHighlightBrush(FSlateNoResource());
	
	StyleInstance->Set("AdvancedRenamer.Style.Splitter", SplitterStyle);

	FSlateBrush* BackgroundBorderBrush = new FSlateColorBrush(FLinearColor::FromSRGBColor(FColor(36, 36, 36)));

	StyleInstance->Set("AdvancedRenamer.Style.BackgroundBorder", BackgroundBorderBrush);

	const FTableViewStyle ListViewStyle = FTableViewStyle()
		.SetBackgroundBrush(*FAdvancedRenamerStyle::Get().GetBrush("AdvancedRenamer.Style.BackgroundBorder"));

	StyleInstance->Set("AdvancedRenamer.Style.ListView", ListViewStyle);

	FHeaderRowStyle HeaderRowStyle = FAppStyle::Get().GetWidgetStyle<FHeaderRowStyle>("TableView.Header");
	HeaderRowStyle.SetHorizontalSeparatorThickness(0);
	HeaderRowStyle.SetHorizontalSeparatorBrush(FSlateNoResource());
	HeaderRowStyle.SetBackgroundBrush(FSlateColorBrush(FLinearColor::FromSRGBColor(FColor(47, 47, 47))));

	StyleInstance->Set("AdvancedRenamer.Style.HeaderRow", HeaderRowStyle);
}

const ISlateStyle& FAdvancedRenamerStyle::Get()
{
	if (!StyleInstance.IsValid())
	{
		Initialize();
	}

	return *StyleInstance;
}
