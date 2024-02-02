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

	/** Material used to render the Slate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	UMaterialInterface* Material = nullptr;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor ButtonMainColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor ButtonShadowColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor ButtonAccentColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor ButtonPressedMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor ButtonShadowMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor BarMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor BarShadowColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor BarAccentColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor HandleMainColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobMainColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobAccentColor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobIndicatorColor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobBarColor;		
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobBarShadowColor;	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobBarFillMinColor;		
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobBarFillMidColor;		
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor KnobBarFillMaxColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor MeterFillMinColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor MeterFillMidColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor MeterFillMaxColor;	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FLinearColor MeterOffFillColor;
	
};
