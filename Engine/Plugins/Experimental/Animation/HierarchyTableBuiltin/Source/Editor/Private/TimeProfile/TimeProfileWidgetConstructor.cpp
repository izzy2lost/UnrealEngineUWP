// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeProfileWidgetConstructor.h"
#include "Widgets/Input/SSpinBox.h"
#include "TimeProfile/HierarchyTableTypeTime.h"
#include "HierarchyTable.h"

TSharedRef<SWidget> FHierarchyTableTimeWidgetConstructor_StartTime::CreateWidget(FHierarchyTableEntryData* EntryData)
{
	return SNew(SSpinBox<float>)
		.IsEnabled_Lambda([EntryData]() { return EntryData->IsOverridden(); })
		.MinDesiredWidth(100.0f)
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.Value_Lambda([EntryData]()
			{
				return EntryData->GetValue<FHierarchyTableType_Time>()->StartTime;
			})
		.OnValueChanged_Lambda([EntryData](float NewValue)
			{
				EntryData->GetMutableValue<FHierarchyTableType_Time>()->StartTime = NewValue;
			});
}

TSharedRef<SWidget> FHierarchyTableTimeWidgetConstructor_EndTime::CreateWidget(FHierarchyTableEntryData* EntryData)
{
	return SNew(SSpinBox<float>)
		.IsEnabled_Lambda([EntryData]() { return EntryData->IsOverridden(); })
		.MinDesiredWidth(100.0f)
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.Value_Lambda([EntryData]()
			{
				return EntryData->GetValue<FHierarchyTableType_Time>()->EndTime;
			})
		.OnValueChanged_Lambda([EntryData](float NewValue)
			{
				EntryData->GetMutableValue<FHierarchyTableType_Time>()->EndTime = NewValue;
			});
}

TSharedRef<SWidget> FHierarchyTableTimeWidgetConstructor_TimeFactor::CreateWidget(FHierarchyTableEntryData* EntryData)
{
	return SNew(SSpinBox<float>)
		.IsEnabled_Lambda([EntryData]() { return EntryData->IsOverridden(); })
		.MinDesiredWidth(100.0f)
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.Value_Lambda([EntryData]()
			{
				return EntryData->GetValue<FHierarchyTableType_Time>()->TimeFactor;
			})
		.OnValueChanged_Lambda([EntryData](float NewValue)
			{
				EntryData->GetMutableValue<FHierarchyTableType_Time>()->TimeFactor = NewValue;
			});
}

TSharedRef<SWidget> FHierarchyTableTimeWidgetConstructor_Preview::CreateWidget(FHierarchyTableEntryData* EntryData)
{
	return SNew(STextBlock)
		.Text(INVTEXT("PREVIEW"));
}