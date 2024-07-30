// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaskProfile/MaskProfileWidgetConstructor.h"
#include "Widgets/Input/SSpinBox.h"
#include "MaskProfile/HierarchyTableTypeMask.h"
#include "HierarchyTable.h"

TSharedRef<SWidget> FHierarchyTableMaskWidgetConstructor_Value::CreateWidget(FHierarchyTableEntryData* EntryData)
{
	return SNew(SSpinBox<float>)
		.IsEnabled_Lambda([EntryData]() { return EntryData->IsOverridden(); })
		.MinDesiredWidth(100.0f)
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.Value_Lambda([EntryData]()
			{
				return EntryData->GetValue<FHierarchyTableType_Mask>()->Value;
			})
		.OnValueChanged_Lambda([EntryData](float NewValue)
			{
				EntryData->GetMutableValue<FHierarchyTableType_Mask>()->Value = NewValue;
			});
}