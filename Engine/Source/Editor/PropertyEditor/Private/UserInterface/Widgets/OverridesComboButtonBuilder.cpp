//  Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterface/Widgets/OverridesComboButtonBuilder.h"
#include "Styling/SlateBrush.h"

FOverridesComboButtonBuilder::FOverridesComboButtonBuilder( TSharedRef<FDetailsDisplayManager> InDetailsDisplayManager):
	FPropertyUpdatedWidgetBuilder(),
	DisplayManager(InDetailsDisplayManager)
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
	static FComboButtonStyle NormalStyle =  FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("DetailsView.OverridesComboButton");
	static FComboButtonStyle HoverStyle =  FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("DetailsView.OverridesComboButton");
	static const FSlateBrush* OverrideOptionsBrush = FAppStyle::GetBrush("DetailsView.OverrideOptions");
	static const FSlateBrush* OverrideHereBrush = FAppStyle::GetBrush("DetailsView.OverrideHere");

	NormalStyle.SetDownArrowImage( *OverrideHereBrush );
	HoverStyle.SetDownArrowImage( *OverrideOptionsBrush );
 
	bool bShouldShowOverrides = true;
	return 	bShouldShowOverrides  ?
				SNew(SHorizontalBox)
				.Visibility(IsVisible)
				+SHorizontalBox::Slot()
					
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew( SComboButton )
					.ComboButtonStyle( &NormalStyle )
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
					.ComboButtonStyle( &HoverStyle )
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
				]
	:
				SNullWidget::NullWidget;
}

TSharedRef<SWidget> FOverridesComboButtonBuilder::operator*()
{
	return GenerateWidget().ToSharedRef();
}

FOverridesComboButtonBuilder::~FOverridesComboButtonBuilder()
{
	OnGetContent.Unbind();
}