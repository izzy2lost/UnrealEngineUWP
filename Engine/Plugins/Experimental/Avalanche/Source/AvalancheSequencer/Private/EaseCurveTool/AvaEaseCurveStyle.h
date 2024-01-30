// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FAvaEaseCurveStyle final : public FSlateStyleSet
{
public:
	static FAvaEaseCurveStyle& Get()
	{
		static FAvaEaseCurveStyle StyleSet;
		return StyleSet;
	}

	FAvaEaseCurveStyle();
	virtual ~FAvaEaseCurveStyle() override;
};
