// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimpleComboButton.h"

#include "SActionButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/SlateTypes.h"
#include "ToolWidgetsStyle.h"

void SSimpleComboButton::Construct(const FArguments& InArgs)
{
	ActionButton = SNew(SActionButton)
		.ActionButtonStyle(&UE::ToolWidgets::FToolWidgetsStyle::Get().GetWidgetStyle<FActionButtonStyle>("SimpleComboButton"))
		.Text(InArgs._Text)
		.TextBlockStyle(
			InArgs._UsesSmallText
			? &FAppStyle::GetWidgetStyle<FTextBlockStyle>("SmallText")
			: &FAppStyle::GetWidgetStyle<FTextBlockStyle>("SmallButtonText"))
		.Icon(InArgs._Icon)
		.MenuContent()
		[
			InArgs._MenuContent.Widget
		]
		.OnGetMenuContent(InArgs._OnGetMenuContent)
		.OnMenuOpenChanged(InArgs._OnMenuOpenChanged)
		.OnComboBoxOpened(InArgs._OnComboBoxOpened);

	ChildSlot
	[
		ActionButton.ToSharedRef()
	];
}

void SSimpleComboButton::SetMenuContentWidgetToFocus(TWeakPtr<SWidget> InWidget)
{
	check(ActionButton.IsValid());

	ActionButton->SetMenuContentWidgetToFocus(InWidget);
}

void SSimpleComboButton::SetIsMenuOpen(bool bInIsOpen, bool bInIsFocused)
{
	check(ActionButton.IsValid());

	ActionButton->SetIsMenuOpen(bInIsOpen, bInIsFocused);
}
