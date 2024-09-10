// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaskProfile/MaskProfileWidgetConstructor.h"
#include "Widgets/Input/SSpinBox.h"
#include "MaskProfile/HierarchyTableTypeMask.h"
#include "HierarchyTable.h"

TSharedRef<SWidget> FHierarchyTableMaskWidgetConstructor_Value::CreateInternalWidget(UHierarchyTable* HierarchyTable, int32 EntryIndex)
{
	return SNew(SSpinBox<float>)
		.IsEnabled_Lambda([HierarchyTable, EntryIndex]() { return HierarchyTable->TableData[EntryIndex].IsOverridden(); })
		.MinDesiredWidth(100.0f)
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.Value_Lambda([HierarchyTable, EntryIndex]()
			{
				return HierarchyTable->TableData[EntryIndex].GetValue<FHierarchyTableType_Mask>()->Value;
			})
		.OnValueChanged_Lambda([HierarchyTable, EntryIndex](float NewValue)
			{
				HierarchyTable->TableData[EntryIndex].GetMutableValue<FHierarchyTableType_Mask>()->Value = NewValue;
			});
}