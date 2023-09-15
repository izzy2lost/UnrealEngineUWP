//  Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsDisplayManager.h"

#include "DetailsViewStyle.h"
#include "SDetailsView.h"

static TAutoConsoleVariable<bool> CVarForceShowComponentEditor(
	TEXT("CoreEntity.UI.ForceShowComponentEditor"),
	true,
	TEXT("Force the component editor to show in the main details tab."));


FDetailsDisplayManager::FDetailsDisplayManager(): bIsOuterCategory(false)
{
	PrimaryStyleKey = SDetailsView::GetPrimaryDetailsViewStyleKey();
}

FDetailsDisplayManager::~FDetailsDisplayManager()
{
	OnDetailsNeedsUpdate.Unbind();
}

bool FDetailsDisplayManager::ShouldHideComponentEditor()
{
	return !GetForceShowSubObjectEditor();
}

bool FDetailsDisplayManager::ShouldShowCategoryMenu()
{
	return false;
}

void FDetailsDisplayManager::SetCategoryObjectName(FName InCategoryObjectName)
{
	CategoryObjectName = InCategoryObjectName;
}

TSharedPtr<SWidget> FDetailsDisplayManager::GetCategoryMenu(FName InCategoryObjectName)
{
	return nullptr;
}

void FDetailsDisplayManager::UpdateView() const
{
	OnDetailsNeedsUpdate.ExecuteIfBound();
}

const FDetailsViewStyleKey& FDetailsDisplayManager::GetDetailsViewStyleKey() const
{
	return PrimaryStyleKey;
}

void FDetailsDisplayManager::SetIsOuterCategory(bool bInIsOuterCategory)
{
	bIsOuterCategory = bInIsOuterCategory;
}

const FDetailsViewStyle* FDetailsDisplayManager::GetDetailsViewStyle() const
{
	const FDetailsViewStyle* ViewStyle = FDetailsViewStyle::GetStyle(GetDetailsViewStyleKey());
	return ViewStyle;
}



FMargin FDetailsDisplayManager::GetTablePadding() const
{
	const FDetailsViewStyle* Style = GetDetailsViewStyle();
	return Style ? Style->GetTablePadding(bIsScrollBarNeeded) : 0;
}

bool FDetailsDisplayManager::GetIsScrollBarNeeded() const
{
	return bIsScrollBarNeeded;
}

void FDetailsDisplayManager::SetIsScrollBarNeeded(bool bInIsScrollBarNeeded)
{
	bIsScrollBarNeeded = bInIsScrollBarNeeded;
}

bool FDetailsDisplayManager::GetForceShowSubObjectEditor()
{
	return CVarForceShowComponentEditor.GetValueOnAnyThread();
}

