// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/OutlinerColumns/ColorPickerOutlinerColumn.h"

namespace UE::Sequencer
{

FColorPickerOutlinerColumn::FColorPickerOutlinerColumn()
{
	Name     = FCommonOutlinerNames::ColorPicker;
	Label    = NSLOCTEXT("FColorPickerOutlinerColumn", "ColorPickerColumnColorPicker", "ColorPicker");
	Position = FOutlinerColumnPosition{ 20, EOutlinerColumnGroup::RightGutter };
	Layout   = FOutlinerColumnLayout{ 6.f, FMargin(0.f), HAlign_Fill, VAlign_Fill, EOutlinerColumnSizeMode::Fixed, EOutlinerColumnFlags::None };
}

} // namespace UE::Sequencer
