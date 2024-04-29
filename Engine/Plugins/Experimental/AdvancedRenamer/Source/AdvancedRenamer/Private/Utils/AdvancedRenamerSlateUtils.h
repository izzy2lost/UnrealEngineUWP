// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Fonts/SlateFontInfo.h"
#include "Layout/Margin.h"
#include "Styling/CoreStyle.h"
#include "UObject/NameTypes.h"

enum class EAdvancedRenamerRemoveOldType : uint8
{
	Separator,
	Chars
};

namespace AdvancedRenamerSlateUtils::Default
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);
	const FSlateFontInfo RegularFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	constexpr float ListLineHeight = 15.f;
	static FName OriginalNameColumnName = TEXT("OriginalName");
	static FName NewNameColumnName = TEXT("NewName");
	static FMargin NameWidgetPadding = FMargin(0.f, 0.f, 4.f, 0.f);
	static FMargin ValueWidgetPadding = FMargin(4.f, 0.f, 0.f, 0.f);
	static FMargin SectionContentFirstEntryPadding = FMargin(8.f, 8.f);
	static FMargin SectionContentMiddleEntriesPadding = FMargin(8.f, 0.f, 8.f, 8.f);
	static FMargin VerticalAddNumberPadding = FMargin(0.f, 4.f);
	static FMargin AddNumberStepPadding = FMargin(8.f, 0.f, 0.f, 0.f);
	static FMargin ChangeCaseFirstButtonPadding = FMargin(0.f, 0.f, 4.f, 0.f);
	static FMargin ChangeCaseMiddleButtonsPadding = FMargin(4.f, 0.f);
	static FMargin ChangeCaseLastButtonPadding = FMargin(4.f, 0.f, 0.f, 0.f);
	static FMargin ApplyButtonPadding = FMargin(8.f, 8.f, 4.f, 8.f);
	static FMargin ResetButtonPadding = FMargin(4.f, 8.f, 4.f, 8.f);
	static FMargin CancelButtonPadding = FMargin(4.f, 8.f, 8.f, 8.f);
}
