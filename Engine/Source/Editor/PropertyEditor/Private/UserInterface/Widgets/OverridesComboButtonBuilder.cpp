//  Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterface/Widgets/OverridesComboButtonBuilder.h"

#include "DetailsViewStyle.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "UObject/OverridableManager.h"

FOverridesComboButtonBuilder::FOverridesComboButtonBuilder(
	TSharedRef<FDetailsDisplayManager> InDetailsDisplayManager,
	bool bInIsCategoryOverridesComboButton,
	TWeakObjectPtr<UObject> InObject ):
	FPropertyUpdatedWidgetBuilder(),
	DisplayManager(InDetailsDisplayManager),
	bIsCategoryOverridesComboButton(bInIsCategoryOverridesComboButton),
    Object(InObject)
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
										&FOverridesWidgetStyleKeys::Here(), bIsCategoryOverridesComboButton ) )
					.Visibility(this, &FOverridesComboButtonBuilder::GetFullyOverridenVisibility)
					.HasDownArrow(true)
				]
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew( SComboButton )
					.ComboButtonStyle( &DetailsViewStyle->GetOverridesComboButtonStyle(
					                                    &FOverridesWidgetStyleKeys::Inside()
                										, bIsCategoryOverridesComboButton ) )
				                    .Visibility(this, &FOverridesComboButtonBuilder::GetOverridenInsideVisibility)
                					.HasDownArrow(true)
				];
}

TSharedRef<SWidget> FOverridesComboButtonBuilder::operator*()
{
	return GenerateWidget().ToSharedRef();
}

FOverridesComboButtonBuilder::~FOverridesComboButtonBuilder()
{
	OnGetContent.Unbind();
}

EVisibility FOverridesComboButtonBuilder::GetOverridenInsideVisibility() const
{
	static FOverridableManager& Manager = FOverridableManager::Get();
	
	if ( UObject* OverridableComponent = Object.Get() )
	{
		const EOverriddenState State = Manager.GetOverriddenState(*OverridableComponent);
		if (State != EOverriddenState::NoOverrides && State != EOverriddenState::AllOverridden)
		{
			return EVisibility::Visible;
		}		
	}
	return EVisibility::Collapsed;
}

EVisibility FOverridesComboButtonBuilder::GetFullyOverridenVisibility() const
{
	if ( UObject* OverridableObject = Object.Get() )
	{
		static FOverridableManager& Manager = FOverridableManager::Get();
		const EOverriddenState State = Manager.GetOverriddenState(*OverridableObject);
		if (State == EOverriddenState::AllOverridden)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}