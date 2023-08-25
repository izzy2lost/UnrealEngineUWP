//  Copyright Epic Games, Inc. All Rights Reserved.


#include "DetailsViewStyle.h"

FDetailsViewStyle::FDetailsViewStyle(
	const FDetailsViewStyleKey& InKey, 
	float InTopCategoryPadding,
	float InHorizontalPadding) :
		FSlateWidgetStyle(),
		Key(InKey),
		TopCategoryPadding(InTopCategoryPadding),
		HorizontalPadding(InHorizontalPadding)
{
}

FDetailsViewStyle::FDetailsViewStyle(FDetailsViewStyleKey& InKey) :
		FSlateWidgetStyle(),
		Key(InKey),
		TopCategoryPadding(0.0f),
		HorizontalPadding(0.0f)
{
	if (const FDetailsViewStyle* Style = GetStyle(InKey))
	{
		Initialize(InKey, Style->HorizontalPadding, Style->TopCategoryPadding);
	}
}

FDetailsViewStyle::FDetailsViewStyle(FDetailsViewStyle& InStyle) :
	FDetailsViewStyle(InStyle.Key, InStyle.TopCategoryPadding, InStyle.HorizontalPadding) 
{
}

FDetailsViewStyle::FDetailsViewStyle(const FDetailsViewStyle& InStyle) :
	FDetailsViewStyle(InStyle.Key, InStyle.TopCategoryPadding, InStyle.HorizontalPadding) 
{
}

FMargin FDetailsViewStyle::GetOuterCategoryRowPadding() const
{
	return FMargin(HorizontalPadding, TopCategoryPadding, HorizontalPadding, 1);
}

FMargin FDetailsViewStyle::GetRowPadding() const
{
	return bIsOuterCategory ?
		FMargin(HorizontalPadding, TopCategoryPadding, HorizontalPadding, 1) :
		FMargin(HorizontalPadding, 0, HorizontalPadding, 1);
}

bool FDetailsViewStyle::operator==(FDetailsViewStyle& OtherLayoutType) const
{
	return Key == OtherLayoutType.Key;
}

FDetailsViewStyle& FDetailsViewStyle::operator=(FDetailsViewStyleKey& OtherLayoutTypeKey)
{
	if (const FDetailsViewStyle* Style = GetStyle(OtherLayoutTypeKey))
	{
		Initialize(OtherLayoutTypeKey, Style->HorizontalPadding, Style->TopCategoryPadding);
	}
	return *this;
}

const FName FDetailsViewStyle::GetTypeName() const
{
	return Key.GetName();
}

const FSlateBrush* FDetailsViewStyle::GetBackgroundImageForCategoryRow(
	const bool bShowBorder, 
	const bool bIsInnerCategory,
	const bool bIsCategoryExpanded,
	const bool bIsScrollBarVisible) const
{
	static const FSlateBrush* InnerCategoryRowBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
	static const FSlateBrush* ClassicStyleTopLevelCategoryRowBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryTop");
	static const FSlateBrush* CardStyleTopLevelCategoryCollapsedScrollbarVisibleRowBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderRounded");
	static const FSlateBrush* CardStyleTopLevelCategoryCollapsedScrollbarHiddenRowBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderLeftSideRounded");
	static const FSlateBrush* CardStyleTopLevelCategoryExpandedScrollbarVisibleRowBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderTopRounded");
	static const FSlateBrush* CardStyleTopLevelCategoryExpandedScrollbarHiddenRowBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderTopLeftSideRounded");

	if (bShowBorder)
	{
		if (bIsInnerCategory)
		{
			return InnerCategoryRowBrush;
		}
			
		const bool bIsCardStyle = Key == FDetailsViewStyleKeys::Card;
			
		if (!bIsCardStyle)
		{
			return ClassicStyleTopLevelCategoryRowBrush;
		}
		if (!bIsCategoryExpanded)
		{
			if (bIsScrollBarVisible)
			{
				return CardStyleTopLevelCategoryCollapsedScrollbarVisibleRowBrush;
			}
			return CardStyleTopLevelCategoryCollapsedScrollbarHiddenRowBrush;
		}
		if (bIsScrollBarVisible)
		{
			return CardStyleTopLevelCategoryExpandedScrollbarVisibleRowBrush;
		}
		return CardStyleTopLevelCategoryExpandedScrollbarHiddenRowBrush;
	}

	return nullptr;
}

void FDetailsViewStyle::InitializeDetailsViewStyles()
{
	static FDetailsViewStyle CardStyle{FDetailsViewStyleKeys::Card, 8.0f, 8.0f};
	StyleKeyToStyleTemplateMap.Add(FDetailsViewStyleKeys::Card.GetName(), &CardStyle);

	static FDetailsViewStyle DefaultStyle{FDetailsViewStyleKeys::Default };
	StyleKeyToStyleTemplateMap.Add(FDetailsViewStyleKeys::Default.GetName(), &DefaultStyle);
	
}

const FSlateBrush* FDetailsViewStyle::GetBackgroundImageForScrollBarWell(
	const bool bShowBorder,
	const bool bIsInnerCategory,
	const bool bIsCategoryExpanded,
	const bool bIsScrollBarVisible) const
{
	static const FSlateBrush* InnerCategoryWellBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
	static const FSlateBrush* ClassicStyleTopLevelCategoryRowBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryTop");
	static const FSlateBrush* DetailsBackgroundWellBrush = FAppStyle::Get().GetBrush("DetailsView.GridLine");
	static const FSlateBrush* CardStyleCollapsedScrollbarVisibleWellBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderRightSideRounded");
	static const FSlateBrush* CardStyleExpandedScrollbarVisibleWellBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderTopRightSideRounded");

	if (bShowBorder)
	{
		if (bIsInnerCategory)
		{
			return InnerCategoryWellBrush;
		}
		const bool bIsCardStyle = this->Key == FDetailsViewStyleKeys::Card;
			
		if (!bIsScrollBarVisible)
		{
			if (!bIsCardStyle)
			{
				return ClassicStyleTopLevelCategoryRowBrush;
			}
			if (!bIsCategoryExpanded){
				return CardStyleCollapsedScrollbarVisibleWellBrush;
			}
			return CardStyleExpandedScrollbarVisibleWellBrush;
		}
		return DetailsBackgroundWellBrush;
	}

	return nullptr;
}

void FDetailsViewStyle::Initialize(
	FDetailsViewStyleKey& InKey,
	const float InHorizontalPadding,
	const float InTopCategoryPadding)
{
	Key = InKey;
	HorizontalPadding = InHorizontalPadding;
	TopCategoryPadding = InTopCategoryPadding;
}

const FDetailsViewStyle* FDetailsViewStyle::GetStyle(FDetailsViewStyleKey& Key)
{
	const FDetailsViewStyle** StylePtr;
		
	StylePtr = StyleKeyToStyleTemplateMap.Find(Key.GetName());
	StylePtr = StylePtr ?
					StylePtr :
					StyleKeyToStyleTemplateMap.Find(FDetailsViewStyleKeys::Default.GetName());
		
	return *StylePtr;
}

void FDetailsViewStyle::SetIsOuterCategory(bool bInIsOuterCategory)
{
	bIsOuterCategory = bInIsOuterCategory;
}