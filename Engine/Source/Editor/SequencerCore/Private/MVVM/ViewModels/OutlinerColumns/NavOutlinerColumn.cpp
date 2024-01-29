// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/OutlinerColumns/NavOutlinerColumn.h"

namespace UE::Sequencer
{

FNavOutlinerColumn::FNavOutlinerColumn()
{
	Name     = FCommonOutlinerNames::Nav;
	Label    = NSLOCTEXT("FNavOutlinerColumn", "NavColumnNav", "Nav");
	Position = FOutlinerColumnPosition{ 10, EOutlinerColumnGroup::RightGutter };
	Layout   = FOutlinerColumnLayout{ 58.f, FMargin(8.f, 0.f), HAlign_Center, VAlign_Center, EOutlinerColumnSizeMode::Fixed, EOutlinerColumnFlags::OverflowSubsequentEmptyCells };
}

} // namespace UE::Sequencer
