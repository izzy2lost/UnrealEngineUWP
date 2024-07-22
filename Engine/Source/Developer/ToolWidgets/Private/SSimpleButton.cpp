// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSimpleButton.h"

#include "SActionButton.h"
#include "ToolWidgetsStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SSimpleButton::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SActionButton)
		.ActionButtonStyle(&UE::ToolWidgets::FToolWidgetsStyle::Get().GetWidgetStyle<FActionButtonStyle>("SimpleButton"))
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(InArgs._Text.IsSet() ? "SimpleButtonLabelAndIcon" : "SimpleButton"))
		.Text(InArgs._Text)
		.Icon(InArgs._Icon)
		.OnClicked(InArgs._OnClicked)
	];
}
