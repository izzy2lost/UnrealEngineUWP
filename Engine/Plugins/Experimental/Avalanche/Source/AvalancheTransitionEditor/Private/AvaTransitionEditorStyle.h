// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaTransitionEditorStyle : public FSlateStyleSet
{
public:
	FAvaTransitionEditorStyle();

	virtual ~FAvaTransitionEditorStyle() override;

	static FAvaTransitionEditorStyle& Get()
	{
		static FAvaTransitionEditorStyle StyleSet;
		return StyleSet;
	}

	static FLinearColor LerpColorSRGB(const FLinearColor& InA, const FLinearColor& InB, float InAlpha);
};
