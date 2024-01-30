// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaEffectorsEditorStyle : public FSlateStyleSet
{
public:
	FAvaEffectorsEditorStyle();
	
	virtual ~FAvaEffectorsEditorStyle() override;
	
	static FAvaEffectorsEditorStyle& Get()
	{
		static FAvaEffectorsEditorStyle StyleSet;
		return StyleSet;
	}
};