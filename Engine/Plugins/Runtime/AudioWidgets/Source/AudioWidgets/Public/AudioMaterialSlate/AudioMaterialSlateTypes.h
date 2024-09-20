// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioWidgetsSlateTypes.h"
#include "Math/Color.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyle.h"
#include "AudioMaterialSlateTypes.generated.h"

/**
 *Base for the appearance of an Audio Material Slates 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialWidgetStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

public:

	FAudioMaterialWidgetStyle();

	/** Material used to render the Slate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = 0), Category = "Style")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	/** Desired Draw size of the rendered material*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayPriority = 1), Category = "Style")
	FVector2f DesiredSize;

public:

	UMaterialInstanceDynamic* CreateDynamicMaterial(UObject* InOuter) const;
	TObjectPtr<UMaterialInstanceDynamic> GetDynamicMaterial() const;

private:

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "keep the reference instead as a TWeakObjectPtr in the AudioMaterialSlates.  This will be removed when all AudioMaterialSlates are updated."))
	mutable TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;
};

/**
 *Represents the appearance of an Audio Material Button 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialButtonStyle : public FAudioMaterialWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialButtonStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialButtonStyle& GetDefault();

	FAudioMaterialButtonStyle& SetMaterial(UMaterialInterface* InMaterialInterface) { Material = InMaterialInterface; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonMainColor;
	FAudioMaterialButtonStyle& SetButtonMainColor(const FLinearColor& InColor) { ButtonMainColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonShadowColor;
	FAudioMaterialButtonStyle& SetButtonShadowColor(const FLinearColor& InColor) { ButtonShadowColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonAccentColor;	
	FAudioMaterialButtonStyle& SetButtonAccentColor(const FLinearColor& InColor) { ButtonShadowColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonPressedMainColor;	
	FAudioMaterialButtonStyle& SetButtonPressedMainColor(const FLinearColor& InColor) { ButtonPressedMainColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonPressedShadowColor;	
	FAudioMaterialButtonStyle& SetButtonPressedShadowColor(const FLinearColor& InColor) { ButtonPressedShadowColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonPressedOutlineColor;
	FAudioMaterialButtonStyle& SetButtonPressedOutlineColor(const FLinearColor& InColor) { ButtonPressedOutlineColor = InColor; return *this; }

};

/**
 *Represents the appearance of an Audio Material Slider 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialSliderStyle : public FAudioMaterialWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialSliderStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialSliderStyle& GetDefault();

	FAudioMaterialSliderStyle& SetMaterial(UMaterialInterface* InMaterialInterface) { Material = InMaterialInterface; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BarMainColor;
	FAudioMaterialSliderStyle& SetSliderBarMainColor(const FLinearColor& InColor) { BarMainColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BarShadowColor;	
	FAudioMaterialSliderStyle& SetSliderBarShadowColor(const FLinearColor& InColor) { BarShadowColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BarAccentColor;
	FAudioMaterialSliderStyle& SetSliderBarAccentColor(const FLinearColor& InColor) { BarAccentColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor HandleMainColor;	
	FAudioMaterialSliderStyle& SetSliderHandleMainColor(const FLinearColor& InColor) { HandleMainColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor HandleOutlineColor;
	FAudioMaterialSliderStyle& SetSliderHandleOutlineColor(const FLinearColor& InColor) { HandleOutlineColor = InColor; return *this; }

	/** The style to use for the audio text box widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FAudioTextBoxStyle TextBoxStyle;
	FAudioMaterialSliderStyle& SetTextBoxStyle(const FAudioTextBoxStyle& InTextBoxStyle) { TextBoxStyle = InTextBoxStyle; return *this; }

};

/**
 *Represents the appearance of an Audio Material Knob 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialKnobStyle : public FAudioMaterialWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialKnobStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialKnobStyle& GetDefault();

	FAudioMaterialKnobStyle& SetMaterial(UMaterialInterface* InMaterialInterface) { Material = InMaterialInterface; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobMainColor;
	FAudioMaterialKnobStyle& SetKnobMainColor(const FLinearColor& InColor) { KnobMainColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobAccentColor;
	FAudioMaterialKnobStyle& SetKnobAccentColor(const FLinearColor& InColor) { KnobAccentColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobIndicatorColor;
	FAudioMaterialKnobStyle& SetKnobIndicatorColor(const FLinearColor& InColor) {KnobIndicatorColor = InColor; return *this;}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarColor;
	FAudioMaterialKnobStyle& SetKnobBarColor(const FLinearColor& InColor) { KnobBarColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarShadowColor;
	FAudioMaterialKnobStyle& SetKnobBarShadowColor(const FLinearColor& InColor) { KnobBarShadowColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillMinColor;
	FAudioMaterialKnobStyle& SetKnobBarFillMinColor(const FLinearColor& InColor) { KnobBarFillMinColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillMidColor;
	FAudioMaterialKnobStyle& SetKnobFillMidColor(const FLinearColor& InColor) { KnobBarFillMidColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillMaxColor;
	FAudioMaterialKnobStyle& SetKnobBarFillMaxColor(const FLinearColor& InColor) { KnobBarFillMaxColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillTintColor;
	FAudioMaterialKnobStyle& SetKnobBarFillTintColor(const FLinearColor& InColor) { KnobBarFillTintColor = InColor; return *this; }

	/** The style to use for the audio text box widget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FAudioTextBoxStyle TextBoxStyle;
	void SetTextBoxStyle(const FAudioTextBoxStyle& InTextBoxStyle) { TextBoxStyle = InTextBoxStyle; }
};

/**
 *Represents the appearance of an Audio Material Meter
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialMeterStyle : public FAudioMaterialWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialMeterStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialMeterStyle& GetDefault();

	FAudioMaterialMeterStyle& SetMaterial(UMaterialInterface* InMaterialInterface) { Material = InMaterialInterface; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterFillMinColor;
	FAudioMaterialMeterStyle& SetMeterFillMinColor(const FLinearColor& InColor) { MeterFillMinColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterFillMidColor;
	FAudioMaterialMeterStyle& SetMeterFillMidColor(const FLinearColor& InColor) { MeterFillMidColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterFillMaxColor;
	FAudioMaterialMeterStyle& SetMeterFillMaxColor(const FLinearColor& InColor) { MeterFillMaxColor = InColor; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterOffFillColor;
	FAudioMaterialMeterStyle& SetMeterOffFillColor(const FLinearColor& InColor) { MeterOffFillColor = InColor; return *this; }

	// How much padding to add around the meter
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D MeterPadding;
	FAudioMaterialMeterStyle& SetMeterpadding(const FVector2D InPadding) { MeterPadding = InPadding; return *this; }

	// The minimum and maximum value to display in dB (values are clamped in this range)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D ValueRangeDb;
	FAudioMaterialMeterStyle& SetValueRangeDb(const FVector2D& InValueRangeDb) { ValueRangeDb = InValueRangeDb; return *this; }

	// Whether or not to show the decibel scale alongside the meter
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool bShowScale;
	FAudioMaterialMeterStyle& SetShowScale(bool bInShowScale) { bShowScale = bInShowScale; return *this; }

	// Which side to show the scale. If vertical, true means left side, false means right side. If horizontal, true means above, false means below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool bScaleSide;
	FAudioMaterialMeterStyle& SetScaleSide(bool bInScaleSide) { bScaleSide = bInScaleSide; return *this; }

	// Offset for the hashes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ScaleHashOffset;
	FAudioMaterialMeterStyle& SetScaleHashOffset(float InScaleHashOffset) { ScaleHashOffset = InScaleHashOffset; return *this; }

	// The width of each hash mark
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ScaleHashWidth;
	FAudioMaterialMeterStyle& SetScaleHashWidth(float InScaleHashWidth) { ScaleHashWidth = InScaleHashWidth; return *this; }

	// The height of each hash mark
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ScaleHashHeight;
	FAudioMaterialMeterStyle& SetScaleHashHeight(float InScaleHashHeight) { ScaleHashHeight = InScaleHashHeight; return *this; }

	// How wide to draw the decibel scale, if it's enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance, meta = (UIMin = "3", ClampMin = "3", UIMax = "10"))
	int32 DecibelsPerHash;
	FAudioMaterialMeterStyle& SetDecibelsPerHash(float InDecibelsPerHash) { DecibelsPerHash = InDecibelsPerHash; return *this; }

	/** Font family and size to be used when displaying the meter scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateFontInfo Font;
	FAudioMaterialMeterStyle& SetFont(const FSlateFontInfo& InFont) { Font = InFont; return *this; }
	FAudioMaterialMeterStyle& SetFont(TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName = NAME_None) { Font = FSlateFontInfo(InCompositeFont, InSize, InTypefaceFontName); return *this; }
	FAudioMaterialMeterStyle& SetFont(const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName = NAME_None) { Font = FSlateFontInfo(InFontObject, InSize, InTypefaceFontName); return *this; }
	FAudioMaterialMeterStyle& SetFont(const FName& InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
	FAudioMaterialMeterStyle& SetFont(const FString& InFontName, uint16 InSize) { Font = FSlateFontInfo(*InFontName, InSize); return *this; }
	FAudioMaterialMeterStyle& SetFont(const WIDECHAR* InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
	FAudioMaterialMeterStyle& SetFont(const ANSICHAR* InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
	FAudioMaterialMeterStyle& SetFontName(const FName& InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMaterialMeterStyle& SetFontName(const FString& InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMaterialMeterStyle& SetFontName(const WIDECHAR* InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMaterialMeterStyle& SetFontName(const ANSICHAR* InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMaterialMeterStyle& SetFontSize(uint16 InSize) { Font.Size = InSize; return *this; }
	FAudioMaterialMeterStyle& SetTypefaceFontName(const FName& InTypefaceFontName) { Font.TypefaceFontName = InTypefaceFontName; return *this; }

};

/**
 *Represents the appearance of an Audio Material Envelope
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialEnvelopeStyle : public FAudioMaterialWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialEnvelopeStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialEnvelopeStyle& GetDefault();

	FAudioMaterialEnvelopeStyle& SetMaterial(UMaterialInterface* InMaterialInterface) { Material = InMaterialInterface; return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor CurveColor;
	FAudioMaterialEnvelopeStyle& SetEnvelopeCurveColor(const FLinearColor& InColor) { CurveColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BackgroundColor;	
	FAudioMaterialEnvelopeStyle& SetEnvelopeBackgroundColor(const FLinearColor& InColor) { BackgroundColor = InColor; return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor OutlineColor;
	FAudioMaterialEnvelopeStyle& SetEnvelopeOutlineColor(const FLinearColor& InColor) { OutlineColor = InColor; return *this; }
	
};