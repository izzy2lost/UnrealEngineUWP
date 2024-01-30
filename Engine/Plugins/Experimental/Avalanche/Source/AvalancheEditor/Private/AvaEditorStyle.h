// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

class FAvaEditorStyle : public FSlateStyleSet
{
public:
	static const FAvaEditorStyle& Get();

	static void Shutdown();

	FAvaEditorStyle();

private:
	static TSharedPtr<FAvaEditorStyle> StyleInstance;

	void Init();
};
