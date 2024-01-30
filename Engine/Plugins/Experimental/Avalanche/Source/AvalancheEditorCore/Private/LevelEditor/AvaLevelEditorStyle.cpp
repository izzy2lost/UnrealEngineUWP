// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaLevelEditorStyle.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

FAvaLevelEditorStyle::FAvaLevelEditorStyle()
	: FSlateStyleSet(TEXT("AvaLevelEditor"))
{
	const FVector2f Icon16x16(16.f, 16.f);

	ContentRootDir     = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	CoreContentRootDir = FPaths::EngineContentDir() / TEXT("Slate");

	Set("AvaLevelEditor.CreateScene"    , new CORE_IMAGE_BRUSH_SVG("Starship/Common/plus", Icon16x16));
	Set("AvaLevelEditor.ActivateScene"  , new CORE_IMAGE_BRUSH_SVG("Starship/Common/play", Icon16x16));
	Set("AvaLevelEditor.DeactivateScene", new CORE_IMAGE_BRUSH_SVG("Starship/Common/stop", Icon16x16));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaLevelEditorStyle::~FAvaLevelEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
