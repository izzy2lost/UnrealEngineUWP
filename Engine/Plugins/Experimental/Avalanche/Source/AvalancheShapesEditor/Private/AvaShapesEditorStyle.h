// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

struct FSlateBrush;

class FAvaShapesEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static FName GetStyleSetName();
	static const ISlateStyle& Get();

	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = nullptr);

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;

	static TSharedRef<FSlateStyleSet> Create();
};
