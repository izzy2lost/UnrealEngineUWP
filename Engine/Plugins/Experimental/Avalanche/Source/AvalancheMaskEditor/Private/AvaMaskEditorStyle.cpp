// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskEditorStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"

namespace UE::AvalancheMaskEditor::Private
{
	static FName NAME_StyleSet = TEXT("AvalancheMaskEditorStyle");
}

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FAvalancheMaskEditorStyle::StyleInstance = nullptr;

void FAvalancheMaskEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FAvalancheMaskEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FAvalancheMaskEditorStyle::GetStyleSetName()
{
	return UE::AvalancheMaskEditor::Private::NAME_StyleSet;
}

const ISlateStyle& FAvalancheMaskEditorStyle::Get()
{
	return *StyleInstance;
}

const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon32x32(32.0f, 32.0f);

TSharedRef<FSlateStyleSet> FAvalancheMaskEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(UE::AvalancheMaskEditor::Private::NAME_StyleSet);

	FString ContentRoot = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetBaseDir() / TEXT("Resources");
	Style->SetContentRoot(ContentRoot);

	Style->Set("AvalancheMaskEditor.ToggleMaskMode.Small", new IMAGE_BRUSH_SVG(TEXT("Icons/MaskIcons/Mode_On"), Icon16x16));
	Style->Set("AvalancheMaskEditor.ToggleMaskMode", new IMAGE_BRUSH_SVG(TEXT("Icons/MaskIcons/Mode_On"), Icon20x20));
	Style->Set("AvalancheMaskEditor.ToggleShowAllMasks", new IMAGE_BRUSH_SVG(TEXT("Icons/MaskIcons/Disable"), Icon20x20));
	Style->Set("AvalancheMaskEditor.ToggleDisableMask", new IMAGE_BRUSH_SVG(TEXT("Icons/MaskIcons/Disable"), Icon20x20));
	Style->Set("AvalancheMaskEditor.ToggleIsolateMask", new IMAGE_BRUSH_SVG(TEXT("Icons/MaskIcons/Disable"), Icon20x20));

	{
		FToolBarStyle ViewportOverlayToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("AssetEditorToolbar");

		ViewportOverlayToolbarStyle.SetButtonPadding(       FMargin(0.0f, 0.0f));
		ViewportOverlayToolbarStyle.SetCheckBoxPadding(     FMargin(0.0f, 0.0f));
		ViewportOverlayToolbarStyle.SetComboButtonPadding(  FMargin(0.0f, 0.0f));
		ViewportOverlayToolbarStyle.SetIndentedBlockPadding(FMargin(0.0f, 0.0f));
		ViewportOverlayToolbarStyle.SetBlockPadding(        FMargin(0.0f, 0.0f));
		ViewportOverlayToolbarStyle.SetSeparatorPadding(    FMargin(0.0f, 0.0f));
		ViewportOverlayToolbarStyle.bShowLabels = false;
		ViewportOverlayToolbarStyle.SetBackground(FSlateColorBrush(FStyleColors::Transparent));
		ViewportOverlayToolbarStyle.SetBackgroundPadding(0);
		ViewportOverlayToolbarStyle.SetButtonPadding(0);
		ViewportOverlayToolbarStyle.SetCheckBoxPadding(0);
		
		Style->Set("AvalancheMaskEditor.ViewportOverlayToolbar", ViewportOverlayToolbarStyle);
	}
	
	return Style;
}

void FAvalancheMaskEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}
