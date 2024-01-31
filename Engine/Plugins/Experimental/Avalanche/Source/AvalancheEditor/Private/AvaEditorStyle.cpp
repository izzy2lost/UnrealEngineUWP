// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorStyle.h"
#include "AvaClonerEffectorShared.h"
#include "Brushes/SlateImageBrush.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "MediaPlate.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FAvaEditorStyle> FAvaEditorStyle::StyleInstance = nullptr;

void FAvaEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

const FAvaEditorStyle& FAvaEditorStyle::Get()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = MakeShared<FAvaEditorStyle>();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}

	return *StyleInstance;
}

FAvaEditorStyle::FAvaEditorStyle()
	: FSlateStyleSet(TEXT("AvalancheEditor"))
{
	Init();
}

namespace UE::AvalancheEditor::Private
{
	const FVector2f Icon16(16.f);
	const FVector2f Icon20(20.f);
	const FVector2f Icon22(22.f);
	const FVector2f Icon25(25.f);
	const FVector2f Icon32(32.f);
	const FVector2f Icon40(40.f);
	const FVector2f Icon64(64.f);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FAvaEditorStyle::Init()
{
	using namespace UE::AvalancheEditor::Private;

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Avalanche"));
	check(Plugin.IsValid());

	if (Plugin.IsValid())
	{
		SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));
	}

	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	// Custom Class Icons
	Set("ClassThumbnail.AvaActor", new IMAGE_BRUSH("Icons/Icon64", Icon64));
	Set("ClassThumbnail.AvalancheBlueprint", new IMAGE_BRUSH("Icons/Icon64", Icon64));
	Set("ClassIcon.AvaActor", new IMAGE_BRUSH("Icons/Icon16", Icon16));
	Set("ClassIcon.AvalancheBlueprint", new IMAGE_BRUSH("Icons/Icon16", Icon16));
	Set("ClassIcon.AvaNullActor", new CORE_IMAGE_BRUSH(TEXT("Icons/SequencerIcons/icon_Sequencer_Move_24x"), Icon16));
	Set("ClassIcon.AvaToolboxStarDynamicMesh", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Favorite", Icon16));
	Set("ClassIcon.AvaToolboxLineDynamicMesh", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/minus", Icon16));
	Set("ClassIcon.AvaToolboxRectangleDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle", Icon16));
	Set("ClassIcon.AvaToolboxNGonDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon", Icon16));
	Set("ClassIcon.AvaToolboxIrregularPolygonDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/irregularpolygon", Icon16));
	Set("ClassIcon.AvaToolboxEllipseDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/circle", Icon16));
	Set("ClassIcon.AvaToolboxRingDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/ring", Icon16));
	Set("ClassIcon.AvaTextActor", new IMAGE_BRUSH("Icons/ToolboxIcons/3d-text", Icon16));
	Set("ClassIcon.AvaShapeActor", new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon", Icon16));
	Set("ClassIcon.AvaClonerActor", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/cloner", Icon16));
	Set("ClassIcon.AvaEffectorActor", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/effector", Icon16));

	// Toolbox Icons
	Set("AvalancheIcons.Chevron",          new IMAGE_BRUSH("Icons/ToolboxIcons/chevron",          Icon16));
	Set("AvalancheIcons.RegularPolygon",   new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon",   Icon16));
	Set("AvalancheIcons.IrregularPolygon", new IMAGE_BRUSH("Icons/ToolboxIcons/irregularpolygon", Icon16));
	Set("AvalancheIcons.Canvas",           new IMAGE_BRUSH("Icons/ToolboxIcons/Canvas",           Icon16));
	Set("AvalancheIcons.LayoutGrid",       new IMAGE_BRUSH("Icons/ToolboxIcons/LayoutGrid",       Icon16));
	Set("AvalancheIcons.Ellipse",          new IMAGE_BRUSH("Icons/ToolboxIcons/circle",           Icon16));
	Set("AvalancheIcons.Ring",             new IMAGE_BRUSH("Icons/ToolboxIcons/ring",             Icon16));
	Set("AvalancheIcons.Arrow",            new IMAGE_BRUSH("Icons/ToolboxIcons/arrow",            Icon16));
	Set("AvalancheIcons.2DArrow",          new IMAGE_BRUSH("Icons/ToolboxIcons/arrow",            Icon16));
	Set("AvalancheIcons.Sphere",           new IMAGE_BRUSH("Icons/ToolboxIcons/sphere",           Icon16));
	Set("AvalancheIcons.Pyramid",          new IMAGE_BRUSH("Icons/ToolboxIcons/pyramid",          Icon16));
	Set("AvalancheIcons.Cone",             new IMAGE_BRUSH("Icons/ToolboxIcons/cone",             Icon16));
	Set("AvalancheIcons.Torus",            new IMAGE_BRUSH("Icons/ToolboxIcons/torus",            Icon16));
	Set("AvalancheIcons.Cube",             new IMAGE_BRUSH("Icons/ToolboxIcons/cube",             Icon16));
	Set("AvalancheIcons.Rectangle",        new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle",        Icon16));
	Set("AvalancheIcons.Plane",            new IMAGE_BRUSH("Icons/ToolboxIcons/plane",            Icon16));
	Set("AvalancheIcons.Cylinder",         new IMAGE_BRUSH("Icons/ToolboxIcons/cylinder",         Icon16));
	Set("AvalancheIcons.Line",             new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/minus",             Icon16));
	Set("AvalancheIcons.Star",             new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Favorite",          Icon16));
	Set("AvalancheIcons.3DText",           new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Text",      Icon16));
	Set("AvalancheIcons.RoundedBox",       new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Primitive", Icon16));
	
	Set("AvalancheIcons.Alignment.Translation.TopLeft",     new IMAGE_BRUSH("Icons/DetailsPanelIcons/TopLeft",     Icon16));
	Set("AvalancheIcons.Alignment.Translation.Top",         new IMAGE_BRUSH("Icons/DetailsPanelIcons/Top",         Icon16));
	Set("AvalancheIcons.Alignment.Translation.TopRight",    new IMAGE_BRUSH("Icons/DetailsPanelIcons/TopRight",    Icon16));
	Set("AvalancheIcons.Alignment.Translation.Left",        new IMAGE_BRUSH("Icons/DetailsPanelIcons/Left",        Icon16));
	Set("AvalancheIcons.Alignment.Translation.Center",      new IMAGE_BRUSH("Icons/DetailsPanelIcons/Center",      Icon16));
	Set("AvalancheIcons.Alignment.Translation.Right",       new IMAGE_BRUSH("Icons/DetailsPanelIcons/Right",       Icon16));
	Set("AvalancheIcons.Alignment.Translation.BottomLeft",  new IMAGE_BRUSH("Icons/DetailsPanelIcons/BottomLeft",  Icon16));
	Set("AvalancheIcons.Alignment.Translation.Bottom",      new IMAGE_BRUSH("Icons/DetailsPanelIcons/Bottom",      Icon16));
	Set("AvalancheIcons.Alignment.Translation.BottomRight", new IMAGE_BRUSH("Icons/DetailsPanelIcons/BottomRight", Icon16));
	
	Set("AvalancheIcons.Alignment.Translation.Back",        new IMAGE_BRUSH_SVG("Icons/DetailsPanelIcons/StackBack",   Icon22));
	Set("AvalancheIcons.Alignment.Translation.Center_X",    new IMAGE_BRUSH_SVG("Icons/DetailsPanelIcons/StackCenter", Icon22));
	Set("AvalancheIcons.Alignment.Translation.Front",       new IMAGE_BRUSH_SVG("Icons/DetailsPanelIcons/StackFront",  Icon22));

	Set("AvalancheIcons.Alignment.Center_YZ",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignCenterVertAndHoriz", Icon22));
	Set("AvalancheIcons.Alignment.Left",        new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignLeft",               Icon22));
	Set("AvalancheIcons.Alignment.Center_Y",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignCenterHoriz",        Icon22));
	Set("AvalancheIcons.Alignment.Right",       new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignRight",              Icon22));
	Set("AvalancheIcons.Alignment.Top",         new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignTop",                Icon22));
	Set("AvalancheIcons.Alignment.Center_Z",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignCenterVert",         Icon22));
	Set("AvalancheIcons.Alignment.Bottom",      new IMAGE_BRUSH_SVG("Icons/PaletteIcons/AlignBottom",             Icon22));
	Set("AvalancheIcons.Alignment.DistributeX", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/DistributeDepth",         Icon22));
	Set("AvalancheIcons.Alignment.DistributeY", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/DistributeHorizontal",    Icon22));
	Set("AvalancheIcons.Alignment.DistributeZ", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/DistributeVertical",      Icon22));
	
	Set("AvalancheIcons.Alignment.Rotation.Actor.Roll",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorRoll",   Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Actor.Pitch",  new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorPitch",  Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Actor.Yaw",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorYaw",    Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Actor.All",    new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignActorAll",    Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Camera.Roll",  new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraRoll",  Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Camera.Pitch", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraPitch", Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Camera.Yaw",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraYaw",   Icon22));
	Set("AvalancheIcons.Alignment.Rotation.Camera.All",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/RotationAlignCameraAll",   Icon22));

	// Grid Icons
	Set("AvalancheIcons.Grid.Toggle",           new IMAGE_BRUSH_SVG("Icons/PaletteIcons/GridToggle",              Icon22));
	Set("AvalancheIcons.Grid.AlwaysShowToggle", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/GridAlwaysVisibleToggle", Icon22));

	// Snap Icons
	Set("AvalancheIcons.Snap.Toggle",       new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapToggle",       Icon22));
	Set("AvalancheIcons.Snap.ToggleGrid",   new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapGridToggle",   Icon22));
	Set("AvalancheIcons.Snap.ToggleActor",  new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapActorToggle",  Icon22));
	Set("AvalancheIcons.Snap.ToggleScreen", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SnapScreenToggle", Icon22));

	// Screen Icons
	Set("AvalancheIcons.Screen.SizeToScreen",        new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SizeToScreen",        Icon22));
	Set("AvalancheIcons.Screen.SizeToScreenStretch", new IMAGE_BRUSH_SVG("Icons/PaletteIcons/SizeToScreenStretch", Icon22));
	Set("AvalancheIcons.Screen.FitToScreen",         new IMAGE_BRUSH_SVG("Icons/PaletteIcons/FitToScreen", Icon22));

	// Logs Icons
	Set("AvalancheIcons.Logs.Pin",   new IMAGE_BRUSH_SVG("Icons/LogsIcons/Pin",   Icon16));
	Set("AvalancheIcons.Logs.Unpin", new IMAGE_BRUSH_SVG("Icons/LogsIcons/Unpin", Icon16));
	
	// Editor Icons
	Set("AvalancheIcons.Editor.ButtonExpander",        new IMAGE_BRUSH("Icons/EditorIcons/ButtonExpander",          FVector2f(7.0f)));
	Set("AvalancheIcons.Editor.ToolHightlight",        new IMAGE_BRUSH("Icons/EditorIcons/ToolIconHighlight",       Icon40));
	Set("AvalancheIcons.Editor.GameView",              new IMAGE_BRUSH_SVG("Icons/EditorIcons/GameView",            Icon25));
	Set("AvalancheIcons.Editor.Visualizers",           new IMAGE_BRUSH_SVG("Icons/EditorIcons/Visualizers",         Icon25));
	Set("AvalancheIcons.Editor.Guides",                new IMAGE_BRUSH_SVG("Icons/EditorIcons/Guides",              Icon25));
	Set("AvalancheIcons.Editor.Billboards",            new IMAGE_BRUSH("Icons/EditorIcons/Billboards",              Icon25));
	Set("AvalancheIcons.Editor.SelectionLock",         new IMAGE_BRUSH("Icons/EditorIcons/SelectionLock",           Icon25));
	Set("AvalancheIcons.Editor.IsolateActors",         new IMAGE_BRUSH("Icons/EditorIcons/IsolateActors",           Icon25));
	Set("AvalancheIcons.Editor.Settings",              new IMAGE_BRUSH("Icons/EditorIcons/Settings",                Icon25));
	Set("AvalancheIcons.Editor.Favorites",             new IMAGE_BRUSH("Icons/EditorIcons/Favorites",               FVector2f(9.0f, 14.0f)));
	Set("AvalancheIcons.Editor.RemoteControlEditor",   new IMAGE_BRUSH("Icons/EditorIcons/RemoteControl",           Icon25));
	Set("AvalancheIcons.Editor.BoundingBoxes",         new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Bounding-Box",   Icon25));
	Set("AvalancheIcons.Editor.SafeFrames",            new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Broadcast-Safe", Icon25));
	Set("AvalancheIcons.Editor.KeyPreview",            new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Key-Preview",    Icon25));
	Set("AvalancheIcons.Editor.Stats",                 new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Performance",    Icon25));
	Set("AvalancheIcons.Editor.Snapshot",              new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Snapshot",       Icon25));
	Set("AvalancheIcons.Editor.WireframeMode",         new IMAGE_BRUSH_SVG("Icons/EditorIcons/Mode_Wireframe",      Icon25));
	Set("AvalancheIcons.Editor.Snap.Toggle",           new IMAGE_BRUSH_SVG("Icons/EditorIcons/Snapping_Toggle",     Icon25));
	Set("AvalancheIcons.Editor.Grid.Toggle",           new IMAGE_BRUSH_SVG("Icons/EditorIcons/Snapping_Grid",       Icon25));
	Set("AvalancheIcons.Editor.Radio.BlackBackground", new IMAGE_BRUSH_SVG("Icons/EditorIcons/radio-background",    Icon16, FStyleColors::Foldout.GetSpecifiedColor()));
	Set("AvalancheIcons.Editor.Mask.Toggle",           new IMAGE_BRUSH("Icons/EditorIcons/Mode_Mask",               Icon25));
	Set("AvalancheIcons.Editor.PivotMode",             new IMAGE_BRUSH("Icons/EditorIcons/Pivot_Mode",              Icon25));
	
	Set("AvalancheIcons.Lock2d", new IMAGE_BRUSH("Icons/DetailsPanelIcons/Lock2d", Icon16));
	Set("AvalancheIcons.Lock3d", new IMAGE_BRUSH("Icons/DetailsPanelIcons/Lock3d", Icon16));
	Set("AvalancheIcons.Unlock", new IMAGE_BRUSH("Icons/DetailsPanelIcons/Unlock", Icon16));

	Set("AvalancheEditor.Thumbnail.Invalid", new FSlateRoundedBoxBrush(FStyleColors::Transparent, 4.0f, FLinearColor(1.0f, 0.2f, 0.2f, 1.0f), 1.0f));

	Set("AvalancheEditor.StaticMeshToolsCategory", new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/StaticMeshActor_16", Icon16));
	Set("AvalancheEditor.CameraToolsCategory",     new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/CameraActor_16", Icon16));
	Set("AvalancheEditor.LightsToolsCategory",     new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/PointLight_16", Icon16));

	Set("AvalancheEditor.CubeTool",     new IMAGE_BRUSH("Icons/ToolboxIcons/cube",     Icon16));
	Set("AvalancheEditor.SphereTool",   new IMAGE_BRUSH("Icons/ToolboxIcons/sphere",   Icon16));
	Set("AvalancheEditor.CylinderTool", new IMAGE_BRUSH("Icons/ToolboxIcons/cylinder", Icon16));
	Set("AvalancheEditor.ConeTool",     new IMAGE_BRUSH("Icons/ToolboxIcons/cone",     Icon20));
	Set("AvalancheEditor.PlaneTool",    new IMAGE_BRUSH("Icons/ToolboxIcons/plane",    Icon20));

	Set("AvalancheEditor.CameraTool",               new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/CameraActor_16",       Icon16));
	Set("AvalancheEditor.CineCameraTool",           new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/CineCameraActor_16",   Icon16));
	Set("AvalancheEditor.CameraRigCraneTool",       new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/CameraRig_Crane_16",   Icon16));
	Set("AvalancheEditor.CameraRigRailTool",        new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/CameraRig_Rail_16",    Icon16));
	Set("AvalancheEditor.CameraShakeSourceTool",    new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/CameraShake",          Icon16));
	Set("AvalancheEditor.AvaPostProcessVolumeTool", new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/PostProcessVolume_16", Icon16));

	Set("AvalancheEditor.PointLightTool",       new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/PointLight_16",       Icon16));
	Set("AvalancheEditor.DirectionalLightTool", new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/DirectionalLight_16", Icon16));
	Set("AvalancheEditor.RectLightTool",        new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/RectLight_16",        Icon16));
	Set("AvalancheEditor.SpotLightTool",        new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/SpotLight_16",        Icon16));
	Set("AvalancheEditor.SkyLightTool",         new CORE_IMAGE_BRUSH_SVG("Starship/AssetIcons/SkyLight_16",         Icon16));

	// Easing
	if (const UEnum* EasingEnum = StaticEnum<EAvaClonerEasing>())
	{
		for (int32 Idx = 0; Idx < EasingEnum->GetMaxEnumValue(); Idx++)
		{
			FText ValueText;
			EasingEnum->GetDisplayValueAsText(static_cast<EAvaClonerEasing>(Idx), ValueText);
			const FString EasingString = ValueText.ToString().Replace(TEXT(" "), TEXT(""));
			const FName StyleName(TEXT("AvalancheIcons.Easing.") + EasingString);
			
			Set(StyleName, new IMAGE_BRUSH_SVG("Icons/ClonerIcons/" + EasingString, Icon32));
		}
	}
	
	// Colors
	Set("AvalancheEditor.PalettesTab.ExpanderHeader", new FSlateColorBrush(FStyleColors::Header));

	// Buttons
	const FTextBlockStyle& AppStyle_ContentBrowserTopBarFont = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("ContentBrowser.TopBar.Font");
	FLinearColor ButtonTextColor = AppStyle_ContentBrowserTopBarFont.ColorAndOpacity.GetSpecifiedColor();
	ButtonTextColor.A /= 2;
	FLinearColor ButtonShadowColorAndOpacity = AppStyle_ContentBrowserTopBarFont.ShadowColorAndOpacity;
	ButtonShadowColorAndOpacity.A /= 2;
	Set("AvalancheEditor.Button.TextStyle", FTextBlockStyle(AppStyle_ContentBrowserTopBarFont)
		.SetColorAndOpacity(ButtonTextColor)
		.SetShadowColorAndOpacity(ButtonShadowColorAndOpacity));

	const FButtonStyle& AppStyle_SimpleButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton");

	Set("AvalancheEditor.BorderlessButton", FButtonStyle(AppStyle_SimpleButton)
		.SetNormalPadding(0.0f)
		.SetPressedPadding(0.0f));

	Set("AvalancheEditor.HighlightButton", FButtonStyle(AppStyle_SimpleButton)
		.SetNormal(FSlateColorBrush(FStyleColors::Secondary))
		.SetHovered(FSlateColorBrush(FStyleColors::Hover))
		.SetPressed(FSlateColorBrush(FStyleColors::Header))
		.SetDisabled(FSlateColorBrush(FStyleColors::Dropdown)));


	Set("AvalancheEditor.SuperBarButton", FButtonStyle(AppStyle_SimpleButton)
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateColorBrush(FStyleColors::Secondary))
		.SetPressed(FSlateColorBrush(FStyleColors::Hover))
		.SetDisabled(FSlateNoResource()));

	Set("AvalancheEditor.SuperBarButton.Rounded", FButtonStyle(AppStyle_SimpleButton)
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateRoundedBoxBrush(FStyleColors::Secondary, 4.0f))
		.SetPressed(FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f))
		.SetDisabled(FSlateNoResource()));

	Set("AvalancheEditor.SuperBarSubMenuButton", FButtonStyle(AppStyle_SimpleButton)
		.SetNormal(FSlateNoResource())
		.SetHovered(FSlateNoResource())
		.SetPressed(FSlateNoResource())
		.SetDisabled(FSlateNoResource()));

	Set("AvalancheEditor.DarkButton", FButtonStyle(AppStyle_SimpleButton)
		.SetNormal(FSlateColorBrush(FStyleColors::Recessed))
		.SetHovered(FSlateColorBrush(FStyleColors::Hover))
		.SetPressed(FSlateColorBrush(FStyleColors::Header))
		.SetDisabled(FSlateColorBrush(FStyleColors::Dropdown)));

	const FButtonStyle& AppStyle_FlatButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton");
	Set("LevelSnapshotsEditor.RemoveFilterButton", FButtonStyle(AppStyle_FlatButton)
		.SetNormal(FSlateNoResource())
		.SetNormalPadding(FMargin(0, 1.5f))
		.SetPressedPadding(FMargin(0, 1.5f)));

	// Text
	const FTextBlockStyle& AppStyle_GraphCompactNodeTitle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("Graph.CompactNode.Title");
	Set("AvalancheEditor.FilterRow.And", FTextBlockStyle(AppStyle_GraphCompactNodeTitle)
		.SetFont(DEFAULT_FONT("BoldCondensed", 16)));
	Set("AvalancheEditor.FilterRow.Or", FTextBlockStyle(AppStyle_GraphCompactNodeTitle)
		.SetFont(DEFAULT_FONT("BoldCondensed", 18)));

	// Check Boxes
	const FCheckBoxStyle& AppStyle_RadioButton = FAppStyle::GetWidgetStyle<FCheckBoxStyle>("RadioButton");
	Set("AvalancheEditor.BlackRadioButton", FCheckBoxStyle(AppStyle_RadioButton)
		.SetBackgroundImage(*GetBrush("AvalancheIcons.Editor.Radio.BlackBackground")));
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
