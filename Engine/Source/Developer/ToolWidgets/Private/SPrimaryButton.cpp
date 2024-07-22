// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPrimaryButton.h"

#include "SActionButton.h"
#include "ToolWidgetsSlateTypes.h"
#include "ToolWidgetsStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SPrimaryButton::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SActionButton)
		.ActionButtonStyle(&UE::ToolWidgets::FToolWidgetsStyle::Get().GetWidgetStyle<FActionButtonStyle>("PrimaryButton"))
		.Text(InArgs._Text)
		.Icon(InArgs._Icon)
		.OnClicked(InArgs._OnClicked)
	];
}
