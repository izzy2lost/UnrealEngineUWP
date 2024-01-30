// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaLevelViewportStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"
#include "Styling/ToolBarStyle.h"

#define LOCTEXT_NAMESPACE "AvaLevelViewportStyle"

TSharedPtr<FSlateStyleSet> FAvaLevelViewportStyle::StyleInstance;

void FAvaLevelViewportStyle::Initialize()
{
	StyleInstance = Create();
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FAvaLevelViewportStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
	StyleInstance.Reset();
}

FName FAvaLevelViewportStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AvalancheLevelViewport"));
	return StyleSetName;
}

const ISlateStyle& FAvaLevelViewportStyle::Get()
{
	if (StyleInstance.IsValid() == false)
	{
		Initialize();
	}

	return *StyleInstance;
}

const FSlateBrush* FAvaLevelViewportStyle::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return StyleInstance->GetBrush(PropertyName, Specifier);
}

#define IMAGE_BRUSH(RelativePath, ... ) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)

TSharedRef<FSlateStyleSet> FAvaLevelViewportStyle::Create()
{
	const FVector2f Icon16x16(16.0f, 16.0f);
	const FVector2f Icon20x20(20.0f, 20.0f);
	const FVector2f Icon22x22(22.0f, 22.0f);
	const FVector2f Icon25x25(25.0f, 25.0f);

	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));
	check(Plugin.IsValid());
	if (Plugin.IsValid())
	{
		Style->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	// Path Constants
	const FString UEContentEditorSlatePath = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	const FString UEContentEditorSlateIconsPath = UEContentEditorSlatePath / TEXT("Icons");

	// Grid Icons
	Style->Set("Button.ToggleGrid", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/GridToggle", Icon22x22));
	Style->Set("Button.ToggleGridAlwaysVisible", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/GridAlwaysVisibleToggle", Icon22x22));

	// Snap Icons
	Style->Set("Button.ToggleSnap", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapToggle", Icon22x22));
	Style->Set("Button.ToggleGrid", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapGridToggle", Icon22x22));
	Style->Set("Button.ToggleScreen", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapScreenToggle", Icon22x22));
	Style->Set("Button.ToggleActor", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapActorToggle", Icon22x22));

	// Viewport
	Style->Set("Button.GameView", new IMAGE_BRUSH_SVG("Icons/EditorIcons/GameView", Icon25x25));
	Style->Set("Button.Visualizers", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Visualizers", Icon25x25));
	Style->Set("Button.Guides", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Guides", Icon25x25));
	Style->Set("Button.Billboards", new IMAGE_BRUSH("Icons/EditorIcons/Billboards", Icon25x25));
	Style->Set("Button.IsolateActors", new IMAGE_BRUSH("Icons/EditorIcons/IsolateActors", Icon25x25));
	Style->Set("Button.BoundingBoxes", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Bounding-Box", Icon25x25));
	Style->Set("Button.SafeFrames", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Broadcast-Safe", Icon25x25));
	Style->Set("Button.KeyPreview", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Key-Preview", Icon25x25));
	Style->Set("Button.Snapshot", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Snapshot", Icon25x25));
	Style->Set("Button.WireframeMode", new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Wireframe", Icon25x25));
	Style->Set("Button.Mask.Toggle", new IMAGE_BRUSH("Icons/EditorIcons/Mode_Mask", Icon25x25));

	// Editor
	Style->Set("Button.SelectionLock", new IMAGE_BRUSH("Icons/EditorIcons/SelectionLock", Icon25x25));
	Style->Set("Button.PivotMode", new IMAGE_BRUSH("Icons/EditorIcons/Pivot_Mode", Icon25x25));

	// Alignment
	Style->Set("AvalancheIcons.Alignment.Translation.TopLeft",     new IMAGE_BRUSH("Icons/DetailsPanelIcons/TopLeft",     Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.Top",         new IMAGE_BRUSH("Icons/DetailsPanelIcons/Top",         Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.TopRight",    new IMAGE_BRUSH("Icons/DetailsPanelIcons/TopRight",    Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.Left",        new IMAGE_BRUSH("Icons/DetailsPanelIcons/Left",        Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.Center",      new IMAGE_BRUSH("Icons/DetailsPanelIcons/Center",      Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.Right",       new IMAGE_BRUSH("Icons/DetailsPanelIcons/Right",       Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.BottomLeft",  new IMAGE_BRUSH("Icons/DetailsPanelIcons/BottomLeft",  Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.Bottom",      new IMAGE_BRUSH("Icons/DetailsPanelIcons/Bottom",      Icon16x16));
	Style->Set("AvalancheIcons.Alignment.Translation.BottomRight", new IMAGE_BRUSH("Icons/DetailsPanelIcons/BottomRight", Icon16x16));
	
	Style->Set("AvalancheIcons.Alignment.Translation.Back",        new IMAGE_BRUSH_SVG("Icons/DetailsPanelIcons/StackBack",   Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Translation.Center_X",    new IMAGE_BRUSH_SVG("Icons/DetailsPanelIcons/StackCenter", Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Translation.Front",       new IMAGE_BRUSH_SVG("Icons/DetailsPanelIcons/StackFront",  Icon22x22));

	Style->Set("AvalancheIcons.Alignment.Center_YZ",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignCenterVertAndHoriz", Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Left",        new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignLeft",               Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Center_Y",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignCenterHoriz",        Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Right",       new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignRight",              Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Top",         new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignTop",                Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Center_Z",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignCenterVert",         Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Bottom",      new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignBottom",             Icon22x22));
	Style->Set("AvalancheIcons.Alignment.DistributeX", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/DistributeDepth",         Icon22x22));
	Style->Set("AvalancheIcons.Alignment.DistributeY", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/DistributeHorizontal",    Icon22x22));
	Style->Set("AvalancheIcons.Alignment.DistributeZ", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/DistributeVertical",      Icon22x22));
	
	Style->Set("AvalancheIcons.Alignment.Rotation.Actor.Roll",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorRoll",   Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Actor.Pitch",  new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorPitch",  Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Actor.Yaw",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorYaw",    Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Actor.All",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorAll",    Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Camera.Roll",  new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraRoll",  Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Camera.Pitch", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraPitch", Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Camera.Yaw",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraYaw",   Icon22x22));
	Style->Set("AvalancheIcons.Alignment.Rotation.Camera.All",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraAll",   Icon22x22));

	// Screen Icons
	Style->Set("AvalancheIcons.Screen.SizeToScreen",        new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SizeToScreen",        Icon22x22));
	Style->Set("AvalancheIcons.Screen.SizeToScreenStretch", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SizeToScreenStretch", Icon22x22));
	Style->Set("AvalancheIcons.Screen.FitToScreen",         new IMAGE_BRUSH_SVG("Icons/PaletteIcons/FitToScreen",         Icon22x22));

	// Color picker icons
	Style->Set("AvalancheIcons.ColorPicker.SolidColors",    new IMAGE_BRUSH("Icons/EditorIcons/SolidColors",    Icon20x20));
	Style->Set("AvalancheIcons.ColorPicker.LinearGradient", new IMAGE_BRUSH("Icons/EditorIcons/LinearGradient", Icon20x20));

	// Post process icons
	Style->Set("AvalancheIcons.PostProcess.RGB",   new IMAGE_BRUSH("Icons/ViewportIcons/RGBSquare", Icon20x20));
	Style->Set("AvalancheIcons.PostProcess.Red",   new IMAGE_BRUSH("Icons/ViewportIcons/RedSquare",   Icon20x20));
	Style->Set("AvalancheIcons.PostProcess.Green", new IMAGE_BRUSH("Icons/ViewportIcons/GreenSquare", Icon20x20));
	Style->Set("AvalancheIcons.PostProcess.Blue",  new IMAGE_BRUSH("Icons/ViewportIcons/BlueSquare",  Icon20x20));
	Style->Set("AvalancheIcons.PostProcess.Alpha", new IMAGE_BRUSH("Icons/ViewportIcons/AlphaSquare", Icon20x20));

	// StatusBar ToolMenu
	{
    	FToolBarStyle StatusToolbarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("AssetEditorToolbar");

    	StatusToolbarStyle.SetButtonPadding(       FMargin(0.0f, 0.0f));
    	StatusToolbarStyle.SetCheckBoxPadding(     FMargin(0.0f, 0.0f));
    	StatusToolbarStyle.SetComboButtonPadding(  FMargin(0.0f, 0.0f));
    	StatusToolbarStyle.SetIndentedBlockPadding(FMargin(0.0f, 0.0f));
    	StatusToolbarStyle.SetBlockPadding(        FMargin(0.0f, 0.0f));
    	StatusToolbarStyle.SetSeparatorPadding(    FMargin(0.0f, 0.0f));
    	StatusToolbarStyle.bShowLabels = false;
    	StatusToolbarStyle.SetBackground(FSlateColorBrush(FStyleColors::Transparent));
    	StatusToolbarStyle.SetBackgroundPadding(0);
    	StatusToolbarStyle.SetButtonPadding(0);
    	StatusToolbarStyle.SetCheckBoxPadding(0);
    	
    	Style->Set("StatusBar", StatusToolbarStyle);
    }

	FCheckBoxStyle CheckBoxStyle = FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("DetailsView.SectionButton");
	CheckBoxStyle.Padding.Left = 10.f;
	CheckBoxStyle.Padding.Right = 10.f;

	Style->Set("Avalanche.Alignment.Context", CheckBoxStyle);
	
	FButtonStyle ButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	ButtonStyle.NormalPadding.Left = 4.f;
	ButtonStyle.NormalPadding.Right = 4.f;
	ButtonStyle.PressedPadding = ButtonStyle.NormalPadding;

	Style->Set("Avalanche.Alignment.Button", ButtonStyle);

	FButtonStyle GuidePresetMenuStyle = FAppStyle::GetWidgetStyle<FButtonStyle>("FlatButton");
	GuidePresetMenuStyle.SetNormalPadding(2.f);
	GuidePresetMenuStyle.SetPressedPadding(2.f);
	GuidePresetMenuStyle.SetDisabled(GuidePresetMenuStyle.Normal);
	GuidePresetMenuStyle.Hovered.TintColor = FSlateColor(EStyleColor::Highlight).GetSpecifiedColor();
	GuidePresetMenuStyle.SetPressed(GuidePresetMenuStyle.Hovered);

	Style->Set("Avalanche.Menu.GuidePreset.Button", GuidePresetMenuStyle);

	return Style;
}

#undef IMAGE_BRUSH
#undef IMAGE_BRUSH_SVG

#undef LOCTEXT_NAMESPACE
