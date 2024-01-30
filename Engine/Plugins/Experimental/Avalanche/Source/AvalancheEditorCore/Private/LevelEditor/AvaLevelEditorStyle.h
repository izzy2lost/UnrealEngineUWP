// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaLevelEditorStyle : public FSlateStyleSet
{
public:
	FAvaLevelEditorStyle();

	virtual ~FAvaLevelEditorStyle() override;

	static FAvaLevelEditorStyle& Get()
	{
		static FAvaLevelEditorStyle StyleSet;
		return StyleSet;
	}
};
