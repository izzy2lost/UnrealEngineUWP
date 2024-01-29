//  Copyright Epic Games, Inc. All Rights Reserved.

#include "UserInterface/Widgets/OverridesComboButtonBuilder.h"

#include "DetailsViewStyle.h"
#include "SSimpleComboButton.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"

FOverridesComboButtonBuilder::FOverridesComboButtonBuilder(
	TSharedRef<FDetailsDisplayManager> InDetailsDisplayManager,
	bool bInIsCategoryOverridesComboButton,
	TWeakObjectPtr<UObject> InObject ):
	FPropertyUpdatedWidgetBuilder(),
	DisplayManager(InDetailsDisplayManager),
	bIsCategoryOverridesComboButton(bInIsCategoryOverridesComboButton),
    Object(InObject),
    EditPropertyChain(nullptr)
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
	const TArray< TSharedRef< const FOverridesWidgetStyleKey >> OverridesWidgetKeys = FOverridesWidgetStyleKeys::GetKeys();
	
	if ( !Object.IsValid() || OverridesWidgetKeys.IsEmpty() )
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SHorizontalBox> Box =	SNew(SHorizontalBox).Visibility(IsVisible);
	
	for (const TSharedRef< const FOverridesWidgetStyleKey >& Key : OverridesWidgetKeys )
	{
		if ( Key->bCanBeVisible )
		{
			Box->AddSlot()
			   .HAlign(HAlign_Right)
			   .VAlign(VAlign_Center)
			   .AutoWidth()
			[
				SNew( SSimpleComboButton )
					.Visibility( Key->GetVisibilityAttribute(EditPropertyChain, Object ))
					.OnGetMenuContent(OnGetContent)
					.Icon(&Key->GetConstStyleBrush())
			];
		}
	}

	return Box.ToSharedPtr();
}

TSharedRef<SWidget> FOverridesComboButtonBuilder::operator*()
{
	return GenerateWidget().ToSharedRef();
}

FOverridesComboButtonBuilder::~FOverridesComboButtonBuilder()
{
	OnGetContent.Unbind();
}

void FOverridesComboButtonBuilder:: SetEditPropertyChain(TSharedRef<FEditPropertyChain>& InEditPropertyChain)
{
	EditPropertyChain = InEditPropertyChain;	
	bIsCategory = false;
}

TSharedPtr<FEditPropertyChain> FOverridesComboButtonBuilder::GetEditPropertyChain() const
{
	return EditPropertyChain;
}
