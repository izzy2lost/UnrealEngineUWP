// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaShapesEditorStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define LOCTEXT_NAMESPACE "AvaShapesEditorStyle"

TSharedPtr<FSlateStyleSet> FAvaShapesEditorStyle::StyleInstance;

void FAvaShapesEditorStyle::Initialize()
{
	StyleInstance = Create();
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FAvaShapesEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
	StyleInstance.Reset();
}

FName FAvaShapesEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("AvalancheShapesEditor"));
	return StyleSetName;
}

const ISlateStyle& FAvaShapesEditorStyle::Get()
{
	if (StyleInstance.IsValid() == false)
	{
		Initialize();
	}

	return *StyleInstance;
}

const FSlateBrush* FAvaShapesEditorStyle::GetBrush(FName PropertyName, const ANSICHAR* Specifier)
{
	return StyleInstance->GetBrush(PropertyName, Specifier);
}

#define IMAGE_BRUSH_SVG(RelativePath, ...) FSlateVectorImageBrush(Style->RootToContentDir(RelativePath, TEXT(".svg")), __VA_ARGS__)
#define IMAGE_BRUSH(RelativePath, ... ) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

TSharedRef<FSlateStyleSet> FAvaShapesEditorStyle::Create()
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

	// Path Constants
	const FString UEContentEditorSlatePath = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	const FString UEContentEditorSlateIconsPath = UEContentEditorSlatePath / TEXT("Icons");

	// Class Icons
	Style->Set("ClassIcon.AvaShape2DArrowDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/arrow", Icon16x16));
	Style->Set("ClassIcon.AvaShapeChevronDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/chevron", Icon16x16));
	Style->Set("ClassIcon.AvaShapeConeDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/cone", Icon16x16));
	Style->Set("ClassIcon.AvaShapeCubeDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/cube", Icon16x16));
	Style->Set("ClassIcon.AvaShapeRoundedPolygonDynamicMesh", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Toolbar_Primitive", Icon16x16));
	Style->Set("ClassIcon.AvaShapeSphereDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/sphere", Icon16x16));
	Style->Set("ClassIcon.AvaShapeTorusDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/torus", Icon16x16));
	Style->Set("ClassIcon.AvaShapeStarDynamicMesh", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Favorite", Icon16x16));
	Style->Set("ClassIcon.AvaShapeLineDynamicMesh", new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/minus", Icon16x16));
	Style->Set("ClassIcon.AvaShapeRectangleDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle", Icon16x16));
	Style->Set("ClassIcon.AvaShapeNGonDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon", Icon16x16));
	Style->Set("ClassIcon.AvaShapeIrregularPolygonDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/irregularpolygon", Icon16x16));
	Style->Set("ClassIcon.AvaShapeEllipseDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/circle", Icon16x16));
	Style->Set("ClassIcon.AvaShapeRingDynamicMesh", new IMAGE_BRUSH("Icons/ToolboxIcons/ring", Icon16x16));
	Style->Set("ClassIcon.AvaShapeShapeActor", new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon", Icon16x16));

	// 2D Tools
	Style->Set("AvalancheShapesEditor.Tool_Shape_2DArrow",       new IMAGE_BRUSH("Icons/ToolboxIcons/arrow", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Chevron",       new IMAGE_BRUSH("Icons/ToolboxIcons/chevron", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Ellipse",       new IMAGE_BRUSH("Icons/ToolboxIcons/circle", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_IrregularPoly", new IMAGE_BRUSH("Icons/ToolboxIcons/irregularpolygon", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Line",          new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/minus", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_NGon",          new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Rectangle",     new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Ring",          new IMAGE_BRUSH("Icons/ToolboxIcons/ring", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Star",          new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Favorite", Icon20x20));

	Style->Set("Tool_Shape_2DArrow",       new IMAGE_BRUSH("Icons/ToolboxIcons/arrow", Icon20x20));
	Style->Set("Tool_Shape_Chevron",       new IMAGE_BRUSH("Icons/ToolboxIcons/chevron", Icon20x20));
	Style->Set("Tool_Shape_Ellipse",       new IMAGE_BRUSH("Icons/ToolboxIcons/circle", Icon20x20));
	Style->Set("Tool_Shape_IrregularPoly", new IMAGE_BRUSH("Icons/ToolboxIcons/irregularpolygon", Icon20x20));
	Style->Set("Tool_Shape_Line",          new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/minus", Icon20x20));
	Style->Set("Tool_Shape_NGon",          new IMAGE_BRUSH("Icons/ToolboxIcons/regularpolygon", Icon20x20));
	Style->Set("Tool_Shape_Rectangle",     new IMAGE_BRUSH("Icons/ToolboxIcons/rectangle", Icon20x20));
	Style->Set("Tool_Shape_Ring",          new IMAGE_BRUSH("Icons/ToolboxIcons/ring", Icon20x20));
	Style->Set("Tool_Shape_Star",          new IMAGE_BRUSH_SVG("Icons/ToolboxIcons/Favorite", Icon20x20));

	// 3D Tools
	Style->Set("AvalancheShapesEditor.Tool_Shape_Cone",   new IMAGE_BRUSH("Icons/ToolboxIcons/cone", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Cube",   new IMAGE_BRUSH("Icons/ToolboxIcons/cube", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Sphere", new IMAGE_BRUSH("Icons/ToolboxIcons/sphere", Icon20x20));
	Style->Set("AvalancheShapesEditor.Tool_Shape_Torus",  new IMAGE_BRUSH("Icons/ToolboxIcons/torus", Icon20x20));

	Style->Set("Tool_Shape_Cone",   new IMAGE_BRUSH("Icons/ToolboxIcons/cone", Icon20x20));
	Style->Set("Tool_Shape_Cube",   new IMAGE_BRUSH("Icons/ToolboxIcons/cube", Icon20x20));
	Style->Set("Tool_Shape_Sphere", new IMAGE_BRUSH("Icons/ToolboxIcons/sphere", Icon20x20));
	Style->Set("Tool_Shape_Torus",  new IMAGE_BRUSH("Icons/ToolboxIcons/torus", Icon20x20));

	return Style;
}

#undef IMAGE_BRUSH_SVG
#undef IMAGE_BRUSH

#undef LOCTEXT_NAMESPACE
