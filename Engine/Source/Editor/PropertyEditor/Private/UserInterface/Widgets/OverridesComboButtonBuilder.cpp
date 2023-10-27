//  Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterface/Widgets/OverridesComboButtonBuilder.h"

#include "DetailsViewStyle.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

FOverridesComboButtonBuilder::FOverridesComboButtonBuilder(
	TSharedRef<FDetailsDisplayManager> InDetailsDisplayManager,
	bool bInIsCategoryOverridesComboButton):
	FPropertyUpdatedWidgetBuilder(),
	DisplayManager(InDetailsDisplayManager),
	bIsCategoryOverridesComboButton(bInIsCategoryOverridesComboButton)
{
}

FOverridesComboButtonBuilder& FOverridesComboButtonBuilder::Set_OnGetContent(FOnGetContent InOnGetContent)
{
	OnGetContent.Unbind();
	OnGetContent = InOnGetContent;
	return *this;
}

TSharedPtr<SWidget> FOverridesComboButtonBuilder::GenerateWidget()
{
    const FDetailsViewStyle* DetailsViewStyle = DisplayManager->GetDetailsViewStyle();

	return	SNew(SHorizontalBox)
				.Visibility(IsVisible)
				+SHorizontalBox::Slot()
					
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew( SComboButton )
					.ComboButtonStyle( &DetailsViewStyle->GetOverridesComboButtonStyle(
										GetOverridesStyleKey(false ), bIsCategoryOverridesComboButton ) )
					.Visibility_Lambda([this]
					{
						if ( IsRowHoveredAttr.IsBound() && IsRowHoveredAttr.Get())
						{
							return EVisibility::Collapsed;
						}
						return EVisibility::Visible;
					})
					.HasDownArrow(true)
					.OnGetMenuContent(OnGetContent)
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew( SComboButton )
					.ComboButtonStyle(  &DetailsViewStyle->GetOverridesComboButtonStyle( GetOverridesStyleKey(true), bIsCategoryOverridesComboButton ))
					.Visibility_Lambda([this]
					{
						if (IsRowHoveredAttr.IsBound() && IsRowHoveredAttr.Get())
						{
							return EVisibility::Visible;
						}
						return EVisibility::Collapsed;
					})
					.HasDownArrow(true)
					.OnGetMenuContent(OnGetContent)
				];
}

TSharedRef<SWidget> FOverridesComboButtonBuilder::operator*()
{
	return GenerateWidget().ToSharedRef();
}

const FOverridesWidgetStyleKey* FOverridesComboButtonBuilder::GetOverridesStyleKey(bool bIsHoveredOver) const
{
	if (bIsCategoryOverridesComboButton && !bIsHoveredOver)
	{
		return &FOverridesWidgetStyleKeys::Here();
	} 
	if (!bIsHoveredOver)
	{
		return &FOverridesWidgetStyleKeys::Here();
	}
	
	return &FOverridesWidgetStyleKeys::Options();
}

FOverridesComboButtonBuilder::~FOverridesComboButtonBuilder()
{
	OnGetContent.Unbind();
}
