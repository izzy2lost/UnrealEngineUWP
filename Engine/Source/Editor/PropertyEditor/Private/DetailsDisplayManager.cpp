//  Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsDisplayManager.h"

#include "DetailsViewStyle.h"


FDetailsDisplayManager::~FDetailsDisplayManager()
{
	OnDetailsNeedsUpdate.Unbind();
}

bool FDetailsDisplayManager::ShouldHideComponentEditor()
{
	return false;
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
	return FDetailsViewStyleKeys::Default();
}

void FDetailsDisplayManager::SetIsOuterCategory(bool bInIsOuterCategory)
{
	bIsOuterCategory = bInIsOuterCategory;
}

FMargin FDetailsDisplayManager::GetTablePadding() const
{
	if (GetDetailsViewStyleKey() == FDetailsViewStyleKeys::Card())
	{
		return bIsScrollbarShowing ?
					FDetailsViewStyle::TablePaddingWithScrollbarCard() :
					FDetailsViewStyle::TablePaddingNoScrollbarCard();
	}
	return bIsScrollbarShowing ?
					FDetailsViewStyle::TablePaddingWithScrollbarClassic() :
					FDetailsViewStyle::TablePaddingNoScrollbarClassic();
		
}

FMargin FDetailsDisplayManager::GetRowPadding() const
{
	FDetailsViewStyle ViewStyle = GetDetailsViewStyleKey();
	ViewStyle.SetIsOuterCategory(bIsOuterCategory);
	ViewStyle.SetIsScrollbarShowing(bIsScrollbarShowing);
	return ViewStyle.GetRowPadding();
}

FMargin FDetailsDisplayManager::GetCategoryButtonsPadding() const
{
	return FMargin(0, 0,  bIsOuterCategory ? 10 : 0, 0);
}

bool FDetailsDisplayManager::GetIsScrollbarShowing() const
{
	return bIsScrollbarShowing;
}

void FDetailsDisplayManager::SetIsScrollbarShowing(bool bInIsScrollbarShowing)
{
	bIsScrollbarShowing = bInIsScrollbarShowing;
}
