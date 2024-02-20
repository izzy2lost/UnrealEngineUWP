// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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

	void CreateDynamicMaterial(UObject* InOuter) const;
	TObjectPtr<UMaterialInstanceDynamic> GetDynamicMaterial() const;

private:

	UPROPERTY()
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