// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutlinerStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateTypes.h"
#include "Styling/ToolBarStyle.h"

TSharedPtr<FSlateStyleSet> FAvaOutlinerStyle::StyleInstance;

void FAvaOutlinerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAvaOutlinerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FAvaOutlinerStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AvalancheOutliner"));
	return StyleSetName;
}

const ISlateStyle& FAvaOutlinerStyle::Get()
{
	return *StyleInstance;
}

const FLinearColor& FAvaOutlinerStyle::GetColor(FName InPropertyName, const ANSICHAR* InSpecifier)
{
	return StyleInstance->GetColor(InPropertyName, InSpecifier);
}

const FSlateBrush* FAvaOutlinerStyle::GetBrush(FName InPropertyName, const ANSICHAR* InSpecifier)
{
	return StyleInstance->GetBrush(InPropertyName, InSpecifier);
}

#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define CORE_IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToCoreContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

const FVector2D Icon8x8(8.f, 8.f);
const FVector2D Icon64x64(64.f, 64.f);
const FVector2D Icon40x40(40.0f, 40.0f);
const FVector2D Icon24x24(24.0f, 24.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon12x12(12.0f, 12.0f);

TSharedRef<FSlateStyleSet> FAvaOutlinerStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));

	check(Plugin.IsValid());
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
		Style->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
	}
	
	Style->Set("AvalancheOutliner.FilterIcon", new IMAGE_BRUSH("Icons/OutlinerIcons/FilterIcon", Icon20x20));
	
	// Table View Row Style
	const FTableRowStyle& TableRowStyle = FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow");
	Style->Set("AvalancheOutliner.TableViewRow", FTableRowStyle(TableRowStyle)
		.SetEvenRowBackgroundBrush(FSlateNoResource())
		.SetOddRowBackgroundBrush(FSlateColorBrush(FStyleColors::Background)));
	
	return Style;
}

#undef BORDER_BRUSH
#undef BOX_BRUSH
#undef CORE_IMAGE_BRUSH
#undef IMAGE_BRUSH
