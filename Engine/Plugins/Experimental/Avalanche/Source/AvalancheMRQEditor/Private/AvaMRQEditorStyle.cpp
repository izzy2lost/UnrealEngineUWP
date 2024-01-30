// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMRQEditorStyle.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"

FAvaMRQEditorStyle::FAvaMRQEditorStyle()
	: FSlateStyleSet(TEXT("AvaMRQEditor"))
{
	const FVector2f Icon20(20.f);

	ContentRootDir     = FPaths::EngineContentDir() / TEXT("Editor/Slate");
	CoreContentRootDir = FPaths::EngineContentDir() / TEXT("Slate");

	Set("AvaMRQEditorCommands.RenderSelectedPages", new IMAGE_BRUSH_SVG("Starship/MainToolbar/cinematics", Icon20));
	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FAvaMRQEditorStyle::~FAvaMRQEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
