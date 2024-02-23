// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"

FAudioMaterialWidgetStyle::FAudioMaterialWidgetStyle()
	:DesiredSize(32.f, 32.f)
{
}

TObjectPtr<UMaterialInstanceDynamic> FAudioMaterialWidgetStyle::GetDynamicMaterial() const
{
	return DynamicMaterial;
}

UMaterialInstanceDynamic* FAudioMaterialWidgetStyle::CreateDynamicMaterial(UObject* InOuter) const
{
	DynamicMaterial = UMaterialInstanceDynamic::Create(Material, InOuter);
	return DynamicMaterial;
}

FAudioMaterialButtonStyle::FAudioMaterialButtonStyle()
	: ButtonMainColor(FLinearColor::Gray)
	, ButtonShadowColor(FLinearColor::Black)
	, ButtonAccentColor(FLinearColor::Gray)
	, ButtonPressedMainColor(FLinearColor::White)
	, ButtonPressedShadowColor(FLinearColor::Gray)
	, ButtonPressedOutlineColor(FLinearColor::Blue)
{
	DesiredSize = FVector2f(128.f, 128.f);
}

const FName FAudioMaterialButtonStyle::TypeName(TEXT("FAudioMaterialButtonStyle"));

void FAudioMaterialButtonStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
}

const FAudioMaterialButtonStyle& FAudioMaterialButtonStyle::GetDefault()
{
	static FAudioMaterialButtonStyle Default;
	return Default;
}

FAudioMaterialSliderStyle::FAudioMaterialSliderStyle()
	: BarMainColor(FLinearColor::White)
	, BarShadowColor(FLinearColor::White)
	, BarAccentColor(FLinearColor::White)
	, HandleMainColor(FLinearColor::White)
	, HandleOutlineColor(FLinearColor::White)
{
	DesiredSize = FVector2f(128.f, 512.f);
}

const FName FAudioMaterialSliderStyle::TypeName(TEXT("FAudioMaterialSliderStyle"));

void FAudioMaterialSliderStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

const FAudioMaterialSliderStyle& FAudioMaterialSliderStyle::GetDefault()
{
	static FAudioMaterialSliderStyle Default;
	return Default;
}

FAudioMaterialKnobStyle::FAudioMaterialKnobStyle()
	: KnobMainColor(FLinearColor::Black)
	, KnobAccentColor(FLinearColor::Gray)
	, KnobIndicatorColor(FLinearColor::Red)
	, KnobBarColor(FLinearColor::Gray)
	, KnobBarShadowColor(FLinearColor::Gray)
	, KnobBarFillMinColor(FLinearColor::White)
	, KnobBarFillMidColor(FLinearColor::White)
	, KnobBarFillMaxColor(FLinearColor::White)
	, KnobBarFillTintColor(FLinearColor::White)
{
	DesiredSize = FVector2f(128.f,128.f);
}

const FName FAudioMaterialKnobStyle::TypeName(TEXT("FAudioMaterialKnobStyle"));

const FAudioMaterialKnobStyle& FAudioMaterialKnobStyle::GetDefault()
{
	static FAudioMaterialKnobStyle Default;
	return Default;
}

void FAudioMaterialKnobStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

FAudioMaterialMeterStyle::FAudioMaterialMeterStyle()
	:MeterFillMinColor(FLinearColor::White)
	, MeterFillMidColor(FLinearColor::White)
	, MeterFillMaxColor(FLinearColor::White)
	, MeterOffFillColor(FLinearColor::Black)
{
	DesiredSize = FVector2f(32.f, 512.f);
}

const FName FAudioMaterialMeterStyle::TypeName(TEXT("FAudioMaterialMeterStyle"));

const FAudioMaterialMeterStyle& FAudioMaterialMeterStyle::GetDefault()
{
	static FAudioMaterialMeterStyle Default;
	return Default;
}

void FAudioMaterialMeterStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

FAudioMaterialEnvelopeStyle::FAudioMaterialEnvelopeStyle()
	: CurveColor(FLinearColor::White)
	, BackgroundColor(FLinearColor::Black)
	, OutlineColor(FLinearColor::Gray)
{
	DesiredSize = FVector2f(256.f, 256.f);
}

const FName FAudioMaterialEnvelopeStyle::TypeName(TEXT("FAudioMaterialEnvelopeStyle"));

const FAudioMaterialEnvelopeStyle& FAudioMaterialEnvelopeStyle::GetDefault()
{
	static FAudioMaterialEnvelopeStyle Default;
	return Default;
}

void FAudioMaterialEnvelopeStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}
