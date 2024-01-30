// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaMRQEditorStyle : public FSlateStyleSet
{
public:
	FAvaMRQEditorStyle();

	virtual ~FAvaMRQEditorStyle() override;

	static FAvaMRQEditorStyle& Get()
	{
		static FAvaMRQEditorStyle StyleSet;
		return StyleSet;
	}
};
