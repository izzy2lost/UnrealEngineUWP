// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaInteractiveToolsStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "AvaInteractiveToolsStyle"

TSharedPtr<FSlateStyleSet> FAvaInteractiveToolsStyle::StyleInstance;

void FAvaInteractiveToolsStyle::Initialize()
{
	StyleInstance = Create();
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FAvaInteractiveToolsStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
	StyleInstance.Reset();
}

FName FAvaInteractiveToolsStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AvalancheInteractiveTools"));
	return StyleSetName;
}

const ISlateStyle& FAvaInteractiveToolsStyle::Get()
{
	if (StyleInstance.IsValid() == false)
	{
		Initialize();
	}

	return *StyleInstance;
}

const FSlateBrush* FAvaInteractiveToolsStyle::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return StyleInstance->GetBrush(PropertyName, Specifier);
}

#define IMAGE_BRUSH(RelativePath, ... ) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)

#define CORE_IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToCoreContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define CORE_IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToCoreContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)

TSharedRef<FSlateStyleSet> FAvaInteractiveToolsStyle::Create()
{
	const FVector2f Icon16x16(16.0f, 16.0f);
	const FVector2f Icon20x20(20.0f, 20.0f);

	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));
	check(Plugin.IsValid());
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	Style->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	// Path Constants
	const FString UEContentEditorSlatePath = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	const FString UEContentEditorSlateIconsPath = UEContentEditorSlatePath / TEXT("Icons");

	// Categories
	Style->Set("AvalancheInteractiveTools.Category_2D",     new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle", Icon20x20));
	Style->Set("AvalancheInteractiveTools.Category_3D",     new IMAGE_BRUSH("Icons/ToolboxIcons/cube", Icon20x20));
	Style->Set("AvalancheInteractiveTools.Category_Actor",  new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/Actor_16", Icon16x16));
	Style->Set("AvalancheInteractiveTools.Category_Layout", new IMAGE_BRUSH("Icons/ToolboxIcons/layoutgrid", Icon20x20));

	// Actor Tools
	Style->Set("AvalancheInteractiveTools.Tool_Actor_Null", new CORE_IMAGE_BRUSH(TEXT("Icons/SequencerIcons/icon_Sequencer_Move_24x"), Icon16x16));
	Style->Set("AvalancheInteractiveTools.Tool_Actor_Spline", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Spline", Icon20x20));
	Style->Set("Tool_Actor_Null", new CORE_IMAGE_BRUSH(TEXT("Icons/SequencerIcons/icon_Sequencer_Move_24x"), Icon20x20));
	Style->Set("Tool_Actor_Spline", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Spline", Icon20x20));

	return Style;
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG
#undef CORE_IMAGE_BRUSH
#undef CORE_IMAGE_BRUSH_SVG

#undef LOCTEXT_NAMESPACE
