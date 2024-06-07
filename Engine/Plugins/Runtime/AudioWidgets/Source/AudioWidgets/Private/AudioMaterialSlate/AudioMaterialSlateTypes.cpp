// Copyright Epic Games, Inc. All Rights Reserved.


#include "AudioMaterialSlate/AudioMaterialSlateTypes.h"
#include "Styling/StyleDefaults.h"

namespace AudioMaterialSlateTypesPrivate
{
	#define PLUGIN_BASE_DIR FString("/AudioWidgets/AudioMaterialSlate/")
}

using namespace AudioMaterialSlateTypesPrivate;

namespace AudioWidgets
{
	namespace SlateTypesPrivate
	{
		static const FLinearColor ButtonMainColor(0.098958f, 0.098958f, 0.098958f, 1.f);
		static const FLinearColor ButtonAccentColor(0.341146f, 0.341146f, 0.341146f, 1.f);
		static const FLinearColor ButtonPressedShadowColor(0.126558f, 0.138653f, 0.15f, 1.f);

		static const FLinearColor BarMainColor(0.008f, 0.008f, 0.008f, 1.f);
		static const FLinearColor BarAccentColor(0.005f, 0.005f, 0.005f, 1.f);
		static const FLinearColor HandleMainColor(1.f, 1.f, 1.f, 1.f);
		static const FLinearColor HandleOutlineColor(0.15f, 0.15f, 0.15f, 1.f);
	}
}

using namespace AudioWidgets;

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
	: ButtonMainColor(SlateTypesPrivate::ButtonMainColor)
	, ButtonShadowColor(FLinearColor::Black)
	, ButtonAccentColor(SlateTypesPrivate::ButtonAccentColor)
	, ButtonPressedMainColor(FLinearColor::White)
	, ButtonPressedShadowColor(SlateTypesPrivate::ButtonPressedShadowColor)
	, ButtonPressedOutlineColor(FLinearColor::Blue)
{
	FString Path = PLUGIN_BASE_DIR + "MI_AudioMaterialButton.MI_AudioMaterialButton";
	Material = LoadObject<UMaterialInterface>(nullptr, *Path);

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
	: BarMainColor(SlateTypesPrivate::BarMainColor)
	, BarShadowColor(FLinearColor::Black)
	, BarAccentColor(SlateTypesPrivate::BarAccentColor)
	, HandleMainColor(SlateTypesPrivate::HandleMainColor)
	, HandleOutlineColor(SlateTypesPrivate::HandleOutlineColor)
	, TextBoxStyle(FAudioTextBoxStyle::GetDefault())
{
	FString Path = PLUGIN_BASE_DIR + "MI_AudioMaterialRoundedSlider.MI_AudioMaterialRoundedSlider";
	Material = LoadObject<UMaterialInterface>(nullptr, *Path);

	DesiredSize = FVector2f(25.f, 250.f);
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
	FString Path = PLUGIN_BASE_DIR + "MI_AudioMaterialKnob.MI_AudioMaterialKnob";
	Material = LoadObject<UMaterialInterface>(nullptr, *Path);

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
	: MeterFillMinColor(FLinearColor::White)
	, MeterFillMidColor(FLinearColor::White)
	, MeterFillMaxColor(FLinearColor::White)
	, MeterOffFillColor(FLinearColor::Black)
	, MeterPadding(FVector2D(10.0f, 5.0f))
	, ValueRangeDb(FVector2D(-60, 10))
	, bShowScale(true)
	, bScaleSide(true)
	, ScaleHashOffset(5.0f)
	, ScaleHashWidth(10.0f)
	, ScaleHashHeight(1.0f)
	, DecibelsPerHash(5)
	, Font(FStyleDefaults::GetFontInfo(5))
{
	FString Path = PLUGIN_BASE_DIR + "MI_AudioMaterialMeter.MI_AudioMaterialMeter";
	Material = LoadObject<UMaterialInterface>(nullptr, *Path);

	DesiredSize = FVector2f(25.f, 512.f);
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
	FString Path = PLUGIN_BASE_DIR + "MI_AudioMaterialEnvelope_ADSR.MI_AudioMaterialEnvelope_ADSR";
	Material = LoadObject<UMaterialInterface>(nullptr, *Path);

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
