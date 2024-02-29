// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMaterialSlate/SAudioMaterialLabeledSlider.h"
#include "AudioMaterialSlate/SAudioMaterialSlider.h"
#include "SAudioTextBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SOverlay.h"

void SAudioMaterialLabeledSlider::Construct(const SAudioMaterialLabeledSlider::FArguments& InArgs)
{
	Style = InArgs._Style;
	OnValueChanged = InArgs._OnValueChanged;
	OnValueCommitted = InArgs._OnValueCommitted;
	Orientation = InArgs._Orientation;
	DesiredSizeOverride = InArgs._DesiredSizeOverride;

	if (InArgs._SliderValue.IsSet())
	{
		SliderValueAttribute = InArgs._SliderValue;
	}

	// Text label
	SAssignNew(Label, SAudioTextBox)
		.Style(&Style->TextBoxStyle)
		.OnValueTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
		{
			const float OutputValue = FCString::Atof(*Text.ToString());
			const float NewSliderValue = GetSliderValueForText(OutputValue);
			if (!FMath::IsNearlyEqual(NewSliderValue, SliderValueAttribute.Get()))
			{
				SliderValueAttribute.Set(NewSliderValue);
				Slider->SetValue(NewSliderValue);
				OnValueChanged.ExecuteIfBound(NewSliderValue);
				OnValueCommitted.ExecuteIfBound(NewSliderValue);
			}
		});

	// Underlying slider widget
	SAssignNew(Slider, SAudioMaterialSlider)
		.ValueAttribute(SliderValueAttribute.Get())
		.Owner(InArgs._Owner)
		.Orientation(Orientation.Get())
		.OnValueChanged_Lambda([this](float Value)
		{
			SliderValueAttribute.Set(Value);
			OnValueChanged.ExecuteIfBound(Value);
			const float OutputValue = GetOutputValueForText(Value);
			Label->SetValueText(OutputValue);
		});

	ChildSlot
	[
		CreateWidgetLayout()
	];
}

void SAudioMaterialLabeledSlider::SetSliderValue(float InSliderValue)
{
	SliderValueAttribute.Set(InSliderValue);
	const float OutputValueForText = GetOutputValueForText(InSliderValue);
	Label->SetValueText(OutputValueForText);
	Slider->SetValue(InSliderValue);
}

void SAudioMaterialLabeledSlider::SetOrientation(EOrientation InOrientation)
{
	SetAttribute(Orientation, TAttribute<EOrientation>(InOrientation), EInvalidateWidgetReason::Layout);
	Slider->SetOrientation(InOrientation);
	LayoutWidgetSwitcher->SetActiveWidgetIndex(Orientation.Get());
}

FVector2D SAudioMaterialLabeledSlider::ComputeDesiredSize(float) const
{
	if (DesiredSizeOverride.Get().IsSet())
	{
		return DesiredSizeOverride.Get().GetValue();
	}
	
	if (Style)
	{
		const float Width = Orientation.Get() == Orient_Vertical ? Style->DesiredSize.X : Style->DesiredSize.Y;
		const float Heigth = Orientation.Get() == Orient_Vertical ? Style->DesiredSize.Y : Style->DesiredSize.X;
		return FVector2D(Width, Heigth);
	}

	return FVector2D::ZeroVector;
}

void SAudioMaterialLabeledSlider::SetDesiredSizeOverride(const FVector2D Size)
{
	SetAttribute(DesiredSizeOverride, TAttribute<TOptional<FVector2D>>(Size), EInvalidateWidgetReason::Layout);
}

const float SAudioMaterialLabeledSlider::GetOutputValue(const float InSliderValue)
{
	return FMath::GetMappedRangeValueClamped(FVector2D(0.0f, 1.0f), OutputRange, InSliderValue);
}

const float SAudioMaterialLabeledSlider::GetOutputValueForText(const float InSliderValue)
{
	return GetOutputValue(InSliderValue);
}

const float SAudioMaterialLabeledSlider::GetSliderValueForText(const float OutputValue)
{
	return GetSliderValue(OutputValue);
}

const float SAudioMaterialLabeledSlider::GetSliderValue(const float OutputValue)
{
	return FMath::GetMappedRangeValueClamped(OutputRange, FVector2D(0.0f, 1.0f) , OutputValue);
}

void SAudioMaterialLabeledSlider::SetOutputRange(const FVector2D Range)
{
	OutputRange = Range;
	// if Range.Y < Range.X, set Range.X to Range.Y
	OutputRange.X = FMath::Min(Range.X, Range.Y);

	const float OutputValue = GetOutputValue(SliderValueAttribute.Get());
	const float ClampedOutputValue = FMath::Clamp(OutputValue, OutputRange.X, OutputRange.Y);
	const float ClampedSliderValue = GetSliderValue(ClampedOutputValue);
	SetSliderValue(ClampedSliderValue);

	Label->UpdateValueTextWidth(OutputRange);
}

void SAudioMaterialLabeledSlider::SetLabelBackgroundColor(FSlateColor InColor)
{
	Label->SetLabelBackgroundColor(InColor.GetSpecifiedColor());
}

void SAudioMaterialLabeledSlider::SetUnitsText(const FText Units)
{
	Label->SetUnitsText(Units);
}

void SAudioMaterialLabeledSlider::SetUnitsTextReadOnly(const bool bIsReadOnly)
{
	Label->SetUnitsTextReadOnly(bIsReadOnly);
}

void SAudioMaterialLabeledSlider::SetValueTextReadOnly(const bool bIsReadOnly)
{
	Label->SetValueTextReadOnly(bIsReadOnly);
}

void SAudioMaterialLabeledSlider::SetShowLabelOnlyOnHover(const bool bShowLabelOnlyOnHover)
{
	Label->SetShowLabelOnlyOnHover(bShowLabelOnlyOnHover);
}

void SAudioMaterialLabeledSlider::SetShowUnitsText(const bool bShowUnitsText)
{
	Label->SetShowUnitsText(bShowUnitsText);
}

TSharedRef<SWidgetSwitcher> SAudioMaterialLabeledSlider::CreateWidgetLayout()
{
	SAssignNew(LayoutWidgetSwitcher, SWidgetSwitcher);
	// Create overall layout
	// Horizontal orientation
	LayoutWidgetSwitcher->AddSlot(EOrientation::Orient_Horizontal)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				// SSlider
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					Slider.ToSharedRef()
				]
			]
			// Text Label
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				Label.ToSharedRef()
			]
		]	
	];
	// Vertical orientation
	LayoutWidgetSwitcher->AddSlot(EOrientation::Orient_Vertical)
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			// Text Label
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				Label.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Fill)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				// Actual SSlider
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				[
					Slider.ToSharedRef()
				]
			]
		]		
	];
	LayoutWidgetSwitcher->SetActiveWidgetIndex(Orientation.Get());
	SetOrientation(Orientation.Get());

	return LayoutWidgetSwitcher.ToSharedRef();
}
