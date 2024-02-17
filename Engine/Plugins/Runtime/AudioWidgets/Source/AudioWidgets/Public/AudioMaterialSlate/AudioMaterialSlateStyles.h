// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/Color.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "AudioMaterialSlateStyles.generated.h"


/**
 *Base for the appearance of an Audio Material Slates 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

public:

	FAudioMaterialStyle();

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
struct AUDIOWIDGETS_API FAudioMaterialButtonStyle : public FAudioMaterialStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialButtonStyle();
	virtual ~FAudioMaterialButtonStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialButtonStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonMainColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonShadowColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonAccentColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonPressedMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonShadowMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor ButtonPressedOutlineColor;

};

/**
 *Represents the appearance of an Audio Material Slider 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialSliderStyle : public FAudioMaterialStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialSliderStyle();
	virtual ~FAudioMaterialSliderStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialSliderStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BarMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BarShadowColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BarAccentColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor HandleMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor HandleOutlineColor;

};

/**
 *Represents the appearance of an Audio Material Knob 
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialKnobStyle : public FAudioMaterialStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialKnobStyle();
	virtual ~FAudioMaterialKnobStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialKnobStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobMainColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobAccentColor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobIndicatorColor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarColor;		
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarShadowColor;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillMinColor;		
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillMidColor;		
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillMaxColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor KnobBarFillTintColor;		
	
};

/**
 *Represents the appearance of an Audio Material Meter
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialMeterStyle : public FAudioMaterialStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialMeterStyle();
	virtual ~FAudioMaterialMeterStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialMeterStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterFillMinColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterFillMidColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterFillMaxColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor MeterOffFillColor;
	
};

/**
 *Represents the appearance of an Audio Material Envelope
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMaterialEnvelopeStyle : public FAudioMaterialStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMaterialEnvelopeStyle();
	virtual ~FAudioMaterialEnvelopeStyle();

	// FSlateWidgetStyle
	virtual void GetResources(TArray<const FSlateBrush*>& OutBrushes) const override;
	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };
	static const FAudioMaterialEnvelopeStyle& GetDefault();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor CurveColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor BackgroundColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style")
	FLinearColor OutlineColor;
	
};
