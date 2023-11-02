// Copyright Epic Games, Inc. All Rights Reserved.

#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"

#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorGlobalLayoutRow.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntity.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorGlobalLayoutBase"

void UDMXControlConsoleEditorGlobalLayoutBase::AddToLayout(UDMXControlConsoleFaderGroup* FaderGroup, const int32 RowIndex, const int32 ColumnIndex)
{
	if (!LayoutRows.IsValidIndex(RowIndex))
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
	if (LayoutRow)
	{
		LayoutRow->Modify();
		LayoutRow->AddToLayoutRow(FaderGroup, ColumnIndex);
	}
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::AddNewRowToLayout(const int32 RowIndex)
{
	if (RowIndex > LayoutRows.Num())
	{
		return nullptr;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
	const int32 ValidRowIndex = RowIndex < 0 ? LayoutRows.Num() : RowIndex;
	LayoutRows.Insert(LayoutRow, ValidRowIndex);

	return LayoutRow;
}

void UDMXControlConsoleEditorGlobalLayoutBase::RemoveFromLayout(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!FaderGroup)
	{
		return;
	}

	const int32 RowIndex = GetFaderGroupRowIndex(FaderGroup);
	const int32 ColumnIndex = GetFaderGroupColumnIndex(FaderGroup);
	if (!LayoutRows.IsValidIndex(RowIndex))
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRows[RowIndex];
	if (LayoutRow)
	{
		LayoutRow->Modify();
		LayoutRow->RemoveFromLayoutRow(FaderGroup);
	}
}

UDMXControlConsoleEditorGlobalLayoutRow* UDMXControlConsoleEditorGlobalLayoutBase::GetLayoutRow(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	const int32 RowIndex = GetFaderGroupRowIndex(FaderGroup);
	return LayoutRows.IsValidIndex(RowIndex) ? LayoutRows[RowIndex] : nullptr;
}

TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> UDMXControlConsoleEditorGlobalLayoutBase::GetAllFaderGroups() const
{
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups;
	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow)
		{
			AllFaderGroups.Append(LayoutRow->GetFaderGroups());
		}
	}

	return AllFaderGroups;
}

void UDMXControlConsoleEditorGlobalLayoutBase::AddToActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup && !ActiveFaderGroups.Contains(FaderGroup))
	{
		ActiveFaderGroups.Add(FaderGroup);
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::RemoveFromActiveFaderGroups(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (FaderGroup && ActiveFaderGroups.Contains(FaderGroup))
	{
		ActiveFaderGroups.Remove(FaderGroup);
	}
}

TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> UDMXControlConsoleEditorGlobalLayoutBase::GetAllActiveFaderGroups() const
{
	TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllActiveFaderGroups = GetAllFaderGroups();
	AllActiveFaderGroups.RemoveAll([](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			return FaderGroup.IsValid() && !FaderGroup->IsActive();
		});

	return AllActiveFaderGroups;
}

void UDMXControlConsoleEditorGlobalLayoutBase::SetActiveFaderGroupsInLayout(bool bActive)
{
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		const bool bActivate = ActiveFaderGroups.Contains(FaderGroup) ? bActive : !bActive;
		FaderGroup->Modify();
		FaderGroup->SetIsActive(bActivate);
	}
}

int32 UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupRowIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (!FaderGroup)
	{
		return INDEX_NONE;
	}

	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow &&
			LayoutRow->GetFaderGroups().Contains(FaderGroup))
		{
			return LayoutRows.IndexOfByKey(LayoutRow);
		}
	}

	return INDEX_NONE;
}

int32 UDMXControlConsoleEditorGlobalLayoutBase::GetFaderGroupColumnIndex(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (!FaderGroup)
	{
		return INDEX_NONE;
	}

	for (const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
	{
		if (LayoutRow && 
			LayoutRow->GetFaderGroups().Contains(FaderGroup))
		{
			return LayoutRow->GetIndex(FaderGroup);
		}
	}

	return INDEX_NONE;
}

void UDMXControlConsoleEditorGlobalLayoutBase::SetLayoutMode(const EDMXControlConsoleLayoutMode NewLayoutMode)
{
	if (LayoutMode == NewLayoutMode)
	{
		return;
	}

	const UDMXControlConsoleEditorLayouts* OwnerEditorLayouts = Cast<UDMXControlConsoleEditorLayouts>(GetOuter());
	if(!ensureMsgf(OwnerEditorLayouts, TEXT("Invalid outer for '%s', cannot set layout mode correctly."), *GetName()))
	{
		return;
	}

	LayoutMode = NewLayoutMode;
	OwnerEditorLayouts->OnLayoutModeChanged.Broadcast();
}

bool UDMXControlConsoleEditorGlobalLayoutBase::ContainsFaderGroup(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	return GetFaderGroupRowIndex(FaderGroup) != INDEX_NONE;
}

void UDMXControlConsoleEditorGlobalLayoutBase::GenerateLayoutByControlConsoleData(const UDMXControlConsoleData* ControlConsoleData)
{
	if (!ControlConsoleData)
	{
		return;
	}

	LayoutRows.Reset(LayoutRows.Num());

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsoleData->GetFaderGroupRows();
	for (const UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = NewObject<UDMXControlConsoleEditorGlobalLayoutRow>(this, NAME_None, RF_Transactional);
		for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroupRow->GetFaderGroups())
		{
			if (FaderGroup)
			{
				LayoutRow->Modify();
				LayoutRow->AddToLayoutRow(FaderGroup);
			}
		}

		LayoutRows.Add(LayoutRow);
	}

	const UDMXControlConsoleEditorLayouts* OwnerEditorLayouts = Cast<UDMXControlConsoleEditorLayouts>(GetOuter());
	if (!ensureMsgf(OwnerEditorLayouts, TEXT("Invalid outer for '%s', cannot get fader group owner correctly."), *GetName()))
	{
		return;
	}

	// The default layout can't contain not patched fader groups
	if (&OwnerEditorLayouts->GetDefaultLayoutChecked() == this)
	{
		CleanLayoutFromUnpatchedFaderGroups();
	}
}

UDMXControlConsoleFaderGroup* UDMXControlConsoleEditorGlobalLayoutBase::FindFaderGroupByFixturePatch(const UDMXEntityFixturePatch* InFixturePatch) const
{
	if (InFixturePatch)
	{
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
		const TWeakObjectPtr<UDMXControlConsoleFaderGroup>* FaderGroupPtr = Algo::FindByPredicate(AllFaderGroups, [InFixturePatch](const TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup)
			{
				return FaderGroup.IsValid() && FaderGroup->GetFixturePatch() == InFixturePatch;
			});

		return FaderGroupPtr ? FaderGroupPtr->Get() : nullptr;
	}

	return nullptr;
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearAll(const bool bOnlyPatchedFaderGroups)
{
	if (bOnlyPatchedFaderGroups)
	{
		for (UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
		{
			if (!LayoutRow)
			{
				continue;
			}

			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();
			for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
			{
				if (FaderGroup.IsValid() &&
					FaderGroup->HasFixturePatch())
				{
					LayoutRow->Modify();
					LayoutRow->RemoveFromLayoutRow(FaderGroup.Get());
				}
			}
		}

		ClearEmptyLayoutRows();
	}
	else
	{
		LayoutRows.Reset();
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::ClearEmptyLayoutRows()
{
	LayoutRows.RemoveAll([](const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
		{
			return LayoutRow && LayoutRow->GetFaderGroups().IsEmpty();
		});
}

void UDMXControlConsoleEditorGlobalLayoutBase::Register(UDMXControlConsoleData* ControlConsoleData)
{
	if (!ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, cannot register layout correctly.")))
	{
		return;
	}

	if (!ensureMsgf(!bIsRegistered, TEXT("Layout already registered to dmx library delegates.")))
	{
		return;
	}

	if (!UDMXLibrary::GetOnEntitiesRemoved().IsBoundToObject(this))
	{
		UDMXLibrary::GetOnEntitiesRemoved().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutBase::OnFixturePatchRemovedFromLibrary);
	}

	if (!ControlConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
	{
		ControlConsoleData->GetOnFaderGroupAdded().AddUObject(this, &UDMXControlConsoleEditorGlobalLayoutBase::OnFaderGroupAddedToData, ControlConsoleData);
	}

	bIsRegistered = true;
}

void UDMXControlConsoleEditorGlobalLayoutBase::Unregister(UDMXControlConsoleData* ControlConsoleData)
{
	if (!ensureMsgf(ControlConsoleData, TEXT("Invalid control console data, cannot register layout correctly.")))
	{
		return;
	}

	if (!ensureMsgf(bIsRegistered, TEXT("Layout already unregistered from dmx library delegates.")))
	{
		return;
	}

	UDMXLibrary::GetOnEntitiesRemoved().RemoveAll(this);

	if (ControlConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
	{
		ControlConsoleData->GetOnFaderGroupAdded().RemoveAll(this);
	}

	bIsRegistered = false;
}

void UDMXControlConsoleEditorGlobalLayoutBase::BeginDestroy()
{
	Super::BeginDestroy();

	ensureMsgf(!bIsRegistered, TEXT("Layout still registered to dmx library delegates before being destroyed."));
}

void UDMXControlConsoleEditorGlobalLayoutBase::OnFixturePatchRemovedFromLibrary(UDMXLibrary* Library, TArray<UDMXEntity*> Entities)
{
	if (Entities.IsEmpty())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (!FaderGroup.IsValid())
		{
			continue;
		}

		const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
		if (!FixturePatch || Entities.Contains(FixturePatch))
		{
			RemoveFromLayout(FaderGroup.Get());
		}
	}

	ClearEmptyLayoutRows();
}

void UDMXControlConsoleEditorGlobalLayoutBase::OnFaderGroupAddedToData(const UDMXControlConsoleFaderGroup* FaderGroup, UDMXControlConsoleData* ControlConsoleData)
{
	if (FaderGroup && FaderGroup->HasFixturePatch())
	{
		Modify();
		GenerateLayoutByControlConsoleData(ControlConsoleData);
	}
}

void UDMXControlConsoleEditorGlobalLayoutBase::CleanLayoutFromUnpatchedFaderGroups()
{
	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = GetAllFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
	{
		if (FaderGroup.IsValid() && !FaderGroup->HasFixturePatch())
		{
			RemoveFromLayout(FaderGroup.Get());
			RemoveFromActiveFaderGroups(FaderGroup.Get());
		}
	}

	ClearEmptyLayoutRows();
}

#undef LOCTEXT_NAMESPACE
