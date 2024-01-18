//  Copyright Epic Games, Inc. All Rights Reserved.


#include "DetailsViewStyle.h"
#include "Containers/Map.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/StyleColors.h"
#include "Styling/StarshipCoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UObject/OverridableManager.h"
#include "Layout/Visibility.h"

TArray< TSharedRef< const FOverridesWidgetStyleKey >> FOverridesWidgetStyleKeys::OverridesWidgetStyleKeys;

const FSlateBrush& FOverridesWidgetStyleKey::GetConstStyleBrush() const
{
	return ImageBrush;
}

const FComboButtonStyle& FOverridesWidgetStyleKey::GetComboButtonStyle( const bool bIsOverridesWidgetForOuterCategory ) const
{
	static TMap<const FOverridesWidgetStyleKey*, const FComboButtonStyle> OverridesKeyToComboButtonStyleMap;
	const FComboButtonStyle* ComboButtonStylePtr = OverridesKeyToComboButtonStyleMap.Find( this );

	if ( ComboButtonStylePtr )
	{
		return *ComboButtonStylePtr;
	}
	
	const FSlateColor BackgroundColor = bIsOverridesWidgetForOuterCategory ? FStyleColors::Header : FStyleColors::Panel;
	static const FSlateColor HoveredBackgroundColor = FStyleColors::Header;
			
	const FButtonStyle OverridesButton = FButtonStyle()
	                                     .SetNormalForeground(FStyleColors::AccentBlue)
	                                     .SetHoveredForeground(FStyleColors::AccentBlue)
	                                     .SetPressedForeground(FStyleColors::AccentBlue)
	                                     .SetHovered(FSlateRoundedBoxBrush(HoveredBackgroundColor, 0.f))
	                                     .SetNormal(FSlateRoundedBoxBrush(BackgroundColor, 0.f))
	                                     .SetPressed(FSlateRoundedBoxBrush(BackgroundColor, 0.f))
	                                     .SetNormalPadding(FMargin(2.f, 0.f, 0.f, 0.f))
	                                     .SetPressedPadding(FMargin(2.f, 0.f, 0.f, 0.f));
			

	OverridesKeyToComboButtonStyleMap.Add( this, FComboButtonStyle(FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FComboButtonStyle>("ComboButton"))
												   .SetButtonStyle(OverridesButton)
												   .SetDownArrowImage(GetConstStyleBrush())
												   .SetDownArrowPadding(FMargin(2.f, 5.f, 3.f, 5.f)));

		
	return *OverridesKeyToComboButtonStyleMap.Find( this );
}

const FOverridesWidgetStyleKey& FOverridesWidgetStyleKeys::Here()
{
	static const FOverridesWidgetStyleKey Here{"OverrideHere", EOverriddenPropertyOperation::Replace, EOverriddenState::AllOverridden };
	return Here;
}

void FOverridesWidgetStyleKeys::Initialize()
{
	OverridesWidgetStyleKeys.Add( MakeShared<FOverridesWidgetStyleKey>(Here()));
	OverridesWidgetStyleKeys.Add( MakeShared<FOverridesWidgetStyleKey>(Inside()));
	OverridesWidgetStyleKeys.Add( MakeShared<FOverridesWidgetStyleKey>(HereInside()));
	OverridesWidgetStyleKeys.Add( MakeShared<FOverridesWidgetStyleKey>(Added()));
	OverridesWidgetStyleKeys.Add( MakeShared<FOverridesWidgetStyleKey>(Removed()));
	OverridesWidgetStyleKeys.Add( MakeShared<FOverridesWidgetStyleKey>(Options()));	
}

const FOverridesWidgetStyleKey& FOverridesWidgetStyleKeys::Added()
{
	static const FOverridesWidgetStyleKey Added{"OverrideAdded"};
	return Added;
}

const FOverridesWidgetStyleKey& FOverridesWidgetStyleKeys::Options()
{
	static const FOverridesWidgetStyleKey Options{"OverrideOptions"};
	return Options;
}

const FOverridesWidgetStyleKey& FOverridesWidgetStyleKeys::Removed()
{
	static const FOverridesWidgetStyleKey Removed{"OverrideRemoved"};
	return Removed;
}

const FOverridesWidgetStyleKey& FOverridesWidgetStyleKeys::Inside()
{
	static const FOverridesWidgetStyleKey Inside{"OverrideInside", EOverriddenPropertyOperation::Modified, EOverriddenState::HasOverrides };
	return Inside;
}

const FOverridesWidgetStyleKey& FOverridesWidgetStyleKeys::HereInside()
{
	static const FOverridesWidgetStyleKey HereInside{"OverrideHereInside" };
	return HereInside;
}

TArray< TSharedRef< const FOverridesWidgetStyleKey >> FOverridesWidgetStyleKeys::GetKeys()
{
	return OverridesWidgetStyleKeys;
}

void FOverridesWidgetStyleKey::Construct()
{
	static const FVector2D Icon16x16{16.0f, 16.0f};
	static const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineContentDir(), TEXT("Slate/Starship/Common/")));
		
	const FSlateVectorImageBrush Brush{Path + Name.ToString() + ".svg", Icon16x16};
	const FSlateBrush* SlateBrushPtr = &Brush;
	ImageBrush = *SlateBrushPtr;
}


TAttribute<EVisibility> FOverridesWidgetStyleKey::GetVisibilityAttribute(const TSharedPtr<FEditPropertyChain>& PropertyChain, TWeakObjectPtr<UObject>& OverriddenObjectWeakPtr) const
{
	static FOverridableManager& Manager = FOverridableManager::Get();
	
	return TAttribute<EVisibility>::CreateLambda( [ this, PropertyChain,  OverriddenObjectWeakPtr ] ()
	{
		const bool bIsProperty = PropertyChain.IsValid();
		
	    if ( OverriddenObjectWeakPtr.IsValid() )
	    {
	    	UObject& OverriddenObject = *OverriddenObjectWeakPtr.Get();

	    	const bool bIsPropertyVisible = bIsProperty &&
												VisibleOverriddenPropertyOperation == Manager.GetOverriddenPropertyOperation(OverriddenObject, *PropertyChain.Get());

			const bool bIsComponentVisible = !bIsProperty && VisibleOverriddenState == Manager.GetOverriddenState(OverriddenObject);

			if ( bIsComponentVisible || bIsPropertyVisible )
			{
				return EVisibility::Visible;
			}
	    }

		return EVisibility::Collapsed;
	});
}

FOverridesWidgetStyleKey::FOverridesWidgetStyleKey(FName InName) : Name{InName}
{
     Construct();
}

FOverridesWidgetStyleKey::FOverridesWidgetStyleKey(FName InName,
																				EOverriddenPropertyOperation InOverriddenPropertyOperation,
																				EOverriddenState InOverriddenState) :
   Name(InName),
   VisibleOverriddenPropertyOperation(InOverriddenPropertyOperation),
   VisibleOverriddenState(InOverriddenState),
   bCanBeVisible(true)
{
	Construct();
}

FDetailsViewStyle::FDetailsViewStyle()
{
}

FDetailsViewStyle::FDetailsViewStyle(
	const FDetailsViewStyleKey& InKey, 
	float InTopCategoryPadding) :
		FSlateWidgetStyle(),
		Key(InKey),
		TopCategoryPadding(InTopCategoryPadding)
{
	Initialize(Key);
}

FMargin FDetailsViewStyle::GetTablePadding(bool bIsScrollBarNeeded) const
{
	if (bIsScrollBarNeeded)
	{
		return TablePaddingWithScrollbar;
	}
	return TablePaddingWithNoScrollbar;
}

void FDetailsViewStyle::Initialize(const FDetailsViewStyle* Style)
{
	if ( Style )
	{
		Key = Style->Key;
		TopCategoryPadding = Style->TopCategoryPadding;
		TablePaddingWithScrollbar = Style->TablePaddingWithScrollbar;
		TablePaddingWithNoScrollbar = Style->TablePaddingWithNoScrollbar;
	}
}

FDetailsViewStyle::FDetailsViewStyle(FDetailsViewStyleKey& InKey) :
		FSlateWidgetStyle(),
		Key(InKey)
{
	Initialize(Key);
}

void FDetailsViewStyle::Initialize(FDetailsViewStyleKey& InKey)
{
	if (const FDetailsViewStyle** Style = StyleKeyToStyleTemplateMap.Find(InKey.GetName()))
	{
		Initialize( *Style );
	}
}

FDetailsViewStyle::FDetailsViewStyle(FDetailsViewStyle& InStyle) :
	FDetailsViewStyle(InStyle.Key, InStyle.TopCategoryPadding ) 
{
}

FDetailsViewStyle::FDetailsViewStyle(const FDetailsViewStyle& InStyle) :
	FDetailsViewStyle(InStyle.Key, InStyle.TopCategoryPadding ) 
{
}

FMargin FDetailsViewStyle::GetOuterCategoryRowPadding() const
{
	return FMargin(0, TopCategoryPadding, 0, 1);
}

FMargin FDetailsViewStyle::GetRowPadding(bool bIsOuterCategory) const
{
	return bIsOuterCategory ?
		FMargin(0, TopCategoryPadding, 0, 1) :
		FMargin(0, 0, 0, 1);
}

bool FDetailsViewStyle::operator==(FDetailsViewStyle& OtherLayoutType) const
{
	return Key == OtherLayoutType.Key;
}

FDetailsViewStyle& FDetailsViewStyle::operator=(FDetailsViewStyleKey& OtherLayoutTypeKey)
{
	if (const FDetailsViewStyle* Style = GetStyle(OtherLayoutTypeKey))
	{
		Initialize(OtherLayoutTypeKey, Style->TopCategoryPadding);
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
	const bool bIsCategoryExpanded) const
{
	static const FSlateBrush* InnerCategoryRowBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
	static const FSlateBrush* ClassicStyleTopLevelCategoryRowBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryTop");
	static const FSlateBrush* CardStyleTopLevelCategoryCollapsedScrollBarNeededRowBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderRounded");
	static const FSlateBrush* CardStyleTopLevelCategoryExpandedScrollBarNeededRowBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderTopRounded");

	if (bShowBorder)
	{
		if (bIsInnerCategory)
		{
			return InnerCategoryRowBrush;
		}
			
		const bool bIsCardStyle = Key == FDetailsViewStyleKeys::Card();
			
		if (!bIsCardStyle)
		{
			return ClassicStyleTopLevelCategoryRowBrush;
		}
		if (!bIsCategoryExpanded)
		{
			return CardStyleTopLevelCategoryCollapsedScrollBarNeededRowBrush;
		}
		return CardStyleTopLevelCategoryExpandedScrollBarNeededRowBrush;
	}

	return nullptr;
}

void FDetailsViewStyle::InitializeDetailsViewStyles()
{
	static FDetailsViewStyle CardStyle{FDetailsViewStyleKeys::Card(), 8.0f};
	CardStyle.TablePaddingWithScrollbar = FMargin(8, 0, 20, 8);
	CardStyle.TablePaddingWithNoScrollbar = FMargin(8, 0, 8, 8);
	StyleKeyToStyleTemplateMap.Add(FDetailsViewStyleKeys::Card().GetName(), &CardStyle);

	static FDetailsViewStyle DefaultStyle{FDetailsViewStyleKeys::Default(), 0.0f };
	StyleKeyToStyleTemplateMap.Add(FDetailsViewStyleKeys::Default().GetName(), &DefaultStyle);
	
	static FDetailsViewStyle ClassicStyle{FDetailsViewStyleKeys::Classic(), 0.0f };
	StyleKeyToStyleTemplateMap.Add(FDetailsViewStyleKeys::Classic().GetName(), &ClassicStyle);

	FOverridesWidgetStyleKeys::Initialize();
}

const FSlateBrush* FDetailsViewStyle::GetBackgroundImageForScrollBarWell(
	const bool bShowBorder,
	const bool bIsInnerCategory,
	const bool bIsCategoryExpanded,
	const bool bIsScrollBarNeeded) const
{
	static const FSlateBrush* InnerCategoryWellBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
	static const FSlateBrush* ClassicStyleTopLevelCategoryRowBrush = FAppStyle::Get().GetBrush("DetailsView.CategoryTop");
	static const FSlateBrush* CardStyleCollapsedScrollBarNeededWellBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderRightSideRounded");
	static const FSlateBrush* CardStyleExpandedScrollBarNeededWellBrush = FAppStyle::Get().GetBrush("DetailsView.CardHeaderTopRightSideRounded");

	if (bShowBorder)
	{
		if (bIsInnerCategory)
		{
			return InnerCategoryWellBrush;
		}
		const bool bIsCardStyle = this->Key == FDetailsViewStyleKeys::Card();
			
		if (!bIsCardStyle)
		{
			return ClassicStyleTopLevelCategoryRowBrush;
		}
		if (!bIsCategoryExpanded){
			return CardStyleCollapsedScrollBarNeededWellBrush;
		}
		return CardStyleExpandedScrollBarNeededWellBrush;
	}

	return nullptr;
}

void FDetailsViewStyle::Initialize(
	FDetailsViewStyleKey& InKey,
	const float InTopCategoryPadding)
{
	Key = InKey;
	TopCategoryPadding = InTopCategoryPadding;
}

const FDetailsViewStyle* FDetailsViewStyle::GetStyle(const FDetailsViewStyleKey& Key)
{
	const FDetailsViewStyle** StylePtr = StyleKeyToStyleTemplateMap.Find(Key.GetName());
	StylePtr = StylePtr ?
					StylePtr :
					StyleKeyToStyleTemplateMap.Find(FDetailsViewStyleKeys::Default().GetName());
		
	return StylePtr ? *StylePtr : nullptr;
}