// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPrimaryButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

void SPrimaryButton::Construct(const FArguments& InArgs)
{
	// To know if the icon is set, check explicitely for a brush or delegate, because
	// if a caller passed nullptr, that would result in _Icon.IsSet() returning true.
	// (Specifically checking IsBound() first so that if it is, we don't invoke it by calling Get())
	const bool bIconSet = InArgs._Icon.IsBound() || InArgs._Icon.Get();

	// If a delegate is bound, we'll store it and pass our own delegate to the Image.
	// This will allow us to update the layout as an icon comes and goes.
	TAttribute<const FSlateBrush*> IconAttribute;
	if (InArgs._Icon.IsBound())
	{
		OwnerDelegate = InArgs._Icon.GetBinding();

		TAttribute<const FSlateBrush*>::FGetter Delegate = TAttribute<const FSlateBrush*>::FGetter::CreateSP(this, &SPrimaryButton::OnGetBrush);		
		IconAttribute.Bind(MoveTemp(Delegate));
	}
	else
	{
		IconAttribute = InArgs._Icon;
	}

	SButton::Construct(SButton::FArguments()
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(bIconSet ? "PrimaryButtonLabelAndIcon" : "PrimaryButton"))
		.OnClicked(InArgs._OnClicked)
		.ForegroundColor(FSlateColor::UseStyle())
		.HAlign(HAlign_Center)
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0,0,bIconSet ? 3 : 0,0))
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(IconAttribute)
				.Visibility(bIconSet ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("PrimaryButtonText"))
				.Text(InArgs._Text)
				.Visibility(InArgs._Text.IsSet() ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			]
		]
	);
}

const FSlateBrush* SPrimaryButton::OnGetBrush()
{
	const FSlateBrush* Brush = nullptr;
	if (OwnerDelegate.IsBound())
	{
		Brush = OwnerDelegate.Execute();
	}

	// Update the button's styling based on whether a brush will be rendered or not
	SetButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>(Brush ? "PrimaryButtonLabelAndIcon" : "PrimaryButton"));
	HorizontalBox->GetSlot(0).SetPadding(FMargin(0,0,Brush ? 3 : 0,0));

	return Brush;
}