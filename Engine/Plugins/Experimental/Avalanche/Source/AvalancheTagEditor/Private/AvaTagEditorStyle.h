// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaTagEditorStyle : public FSlateStyleSet
{
public:
	FAvaTagEditorStyle();

	virtual ~FAvaTagEditorStyle() override;

	static FAvaTagEditorStyle& Get()
	{
		static FAvaTagEditorStyle StyleSet;
		return StyleSet;
	}
};
