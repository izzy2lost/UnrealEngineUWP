// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialSlateStyles.h"

FAudioMaterialStyle::FAudioMaterialStyle()
	:DesiredSize(32.f, 32.f)
{
}

TObjectPtr<UMaterialInstanceDynamic> FAudioMaterialStyle::GetDynamicMaterial() const
{
	return DynamicMaterial;
}

void FAudioMaterialStyle::CreateDynamicMaterial(UObject* InOuter) const
{
	DynamicMaterial = UMaterialInstanceDynamic::Create(Material, InOuter);
}

void FAudioMaterialSliderStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}

FAudioMaterialButtonStyle::FAudioMaterialButtonStyle()
	:ButtonMainColor(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
	,ButtonShadowColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
	,ButtonAccentColor(FLinearColor(0.34f, 0.34f, 0.34f, 1.0f))
	,ButtonPressedMainColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f))
	,ButtonShadowMainColor(FLinearColor(0.156f, 0.138f, 0.15f, 1.0f))
	,ButtonPressedOutlineColor(FLinearColor(0.156f, 0.138f, 0.15f, 1.0f))
{
	DesiredSize = FVector2f(128.f, 128.f);
}

FAudioMaterialButtonStyle::~FAudioMaterialButtonStyle()
{
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
	:BarMainColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f))
	,BarShadowColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f))
	,BarAccentColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f))
	,HandleMainColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f))
	,HandleOutlineColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.0f))
{
	DesiredSize = FVector2f(128.f, 512.f);
}

FAudioMaterialSliderStyle::~FAudioMaterialSliderStyle()
{
}

const FName FAudioMaterialSliderStyle::TypeName(TEXT("FAudioMaterialSliderStyle"));

const FAudioMaterialSliderStyle& FAudioMaterialSliderStyle::GetDefault()
{
	static FAudioMaterialSliderStyle Default;
	return Default;
}

FAudioMaterialKnobStyle::FAudioMaterialKnobStyle()
	:KnobMainColor(FLinearColor(0.f, 0.f, 0.f, 1.0f))
	,KnobAccentColor(FLinearColor(.14f, .16f, .2f, 1.0f))
	,KnobIndicatorColor(FLinearColor(1.f, 0.f, 0.04f, 1.0f))
	,KnobBarColor(FLinearColor(.033f, .033f, .033f, 1.0f))
	,KnobBarShadowColor(FLinearColor(0.f, 0.f, 0.f, 1.0f))
	,KnobBarFillMinColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
	,KnobBarFillMidColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
	,KnobBarFillMaxColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
	,KnobBarFillTintColor(FLinearColor(1.f, 1.f, 1.f, 1.0f))
{
	DesiredSize = FVector2f(256.f,256.f);
}

FAudioMaterialKnobStyle::~FAudioMaterialKnobStyle()
{
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
	:MeterFillMinColor(FLinearColor(1.f, 1.f, 1.f, 1.f))
	,MeterFillMidColor(FLinearColor(1.f, 1.f, 1.f, 1.f))
	,MeterFillMaxColor(FLinearColor(1.f, 1.f, 1.f, 1.f))
	,MeterOffFillColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f))
{
	DesiredSize = FVector2f(32.f, 512.f);
}

FAudioMaterialMeterStyle::~FAudioMaterialMeterStyle()
{
}

const FName FAudioMaterialMeterStyle::TypeName(TEXT("FAudioMaterialKnobStyle"));

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
	:CurveColor(FLinearColor(0.15f, 0.45f, 0.75f, 1.f))
	,BackgroundColor(FLinearColor(0.01f, 0.01f, 0.01f, 1.f))
	,OutlineColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.f))
{
	DesiredSize = FVector2f(256.f, 256.f);
}

FAudioMaterialEnvelopeStyle::~FAudioMaterialEnvelopeStyle()
{
}

const FName FAudioMaterialEnvelopeStyle::TypeName(TEXT("FAudioMaterialKnobStyle"));

const FAudioMaterialEnvelopeStyle& FAudioMaterialEnvelopeStyle::GetDefault()
{
	static FAudioMaterialEnvelopeStyle Default;
	return Default;
}

void FAudioMaterialEnvelopeStyle::GetResources(TArray<const FSlateBrush*>& OutBrushes) const
{
	// Add any brush resources here so that Slate can correctly atlas and reference them
}
