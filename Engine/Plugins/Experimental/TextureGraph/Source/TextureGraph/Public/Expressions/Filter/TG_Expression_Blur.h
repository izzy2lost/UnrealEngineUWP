// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Expressions/TG_Expression_MaterialBase.h"

#include "TG_Expression_Blur.generated.h"

UENUM(BlueprintType)
enum class EBlurType : uint8
{
	Gaussian = 0		UMETA(DisplayName = "Gaussian"),
	Directional	= 1		UMETA(DisplayName = "Directional"),
	Radial = 2			UMETA(DisplayName = "Radial")
};

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_Blur : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_EXPRESSION(TG_Category::Filter);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = 0, ClampMax = 100, EditCondition="BlurType==EBlurType::Gaussian || BlurType==EBlurType::Radial", EditConditionHides))
	int32 Radius = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = -180, ClampMax = 180, EditCondition="BlurType==EBlurType::Directional", EditConditionHides))
	float Angle = 0.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting", ClampMin = 0, ClampMax = 1, EditCondition="BlurType==EBlurType::Directional || BlurType==EBlurType::Radial", EditConditionHides))
	float Strength = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Setting"))
	EBlurType BlurType = EBlurType::Gaussian;

	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = ""))
	FTG_Texture Output;

	UPROPERTY(meta = (TGType = "TG_Input"))
	FTG_Texture Input;

	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Applies a blur filter with a given strength.")); }
	virtual void Evaluate(FTG_EvaluationContext* InContext) override;
};

