// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorGlobalLayoutRow.h"

#include "DMXControlConsoleFaderGroup.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorGlobalLayoutRow"

UDMXControlConsoleEditorGlobalLayoutRow::FOnGlobalLayoutRowChangedDelegate UDMXControlConsoleEditorGlobalLayoutRow::OnGlobalLayoutRowChanged;

void UDMXControlConsoleEditorGlobalLayoutRow::AddToLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup, const int32 Index)
{
	if (!FaderGroup || Index > FaderGroups.Num())
	{
		return;
	}

	const int32 ValidIndex = Index < 0 ? FaderGroups.Num() : Index;
	FaderGroups.Insert(FaderGroup, ValidIndex);

	OnGlobalLayoutRowChanged.Broadcast(this);
}

void UDMXControlConsoleEditorGlobalLayoutRow::AddToLayoutRow(const TArray<UDMXControlConsoleFaderGroup*> InFaderGroups)
{
	if (!InFaderGroups.IsEmpty())
	{
		FaderGroups.Append(InFaderGroups);

		OnGlobalLayoutRowChanged.Broadcast(this);
	}
}

void UDMXControlConsoleEditorGlobalLayoutRow::RemoveFromLayoutRow(UDMXControlConsoleFaderGroup* FaderGroup)
{
	FaderGroups.Remove(FaderGroup);

	OnGlobalLayoutRowChanged.Broadcast(this);
}

int32 UDMXControlConsoleEditorGlobalLayoutRow::GetRowIndex() const
{
	const UDMXControlConsoleEditorGlobalLayoutBase& OwnerLayout = GetOwnerLayoutChecked();

	const TArray<UDMXControlConsoleEditorGlobalLayoutRow*> LayoutRows = OwnerLayout.GetLayoutRows();
	return LayoutRows.IndexOfByKey(this);
}

UDMXControlConsoleEditorGlobalLayoutBase& UDMXControlConsoleEditorGlobalLayoutRow::GetOwnerLayoutChecked() const
{
	UDMXControlConsoleEditorGlobalLayoutBase* Outer = Cast<UDMXControlConsoleEditorGlobalLayoutBase>(GetOuter());
	checkf(Outer, TEXT("Invalid outer for '%s', cannot get layout row owner correctly."), *GetName());

	return *Outer;
}

int32 UDMXControlConsoleEditorGlobalLayoutRow::GetIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	return FaderGroups.IndexOfByKey(FaderGroup);
}

#undef LOCTEXT_NAMESPACE
