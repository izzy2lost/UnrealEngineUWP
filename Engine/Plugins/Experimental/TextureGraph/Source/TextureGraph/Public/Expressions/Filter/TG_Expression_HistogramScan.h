// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Expressions/TG_Expression.h"
#include "TG_Texture.h"
#include "FxMat/FxMaterial.h"

#include "TG_Expression_HistogramScan.generated.h"

//////////////////////////////////////////////////////////////////////////
/// Expression
//////////////////////////////////////////////////////////////////////////
UCLASS()
class TEXTUREGRAPH_API UTG_Expression_HistogramScan : public UTG_Expression
{
	GENERATED_BODY()

public:
	TG_DECLARE_EXPRESSION(TG_Category::Adjustment);
	virtual void							Evaluate(FTG_EvaluationContext* InContext) override;

	// Drives the position of the histogram of the input image
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
		float								Position = 0;

	// Drives the contrast of the histogram of the input image
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Setting", UIMin = "0", ClampMin = "0", UIMax = "1", ClampMax = "1"))
		float								Contrast = 0;

	// The input texture to adjust the brightness/contrast for
	UPROPERTY(meta = (TGType = "TG_Input", PinDisplayName = ""))
		FTG_Texture							Input;

	// The output image
	UPROPERTY(EditAnywhere, Category = NoCategory, meta = (TGType = "TG_Output", PinDisplayName = ""))
		FTG_Texture							Output;
	virtual FTG_Name						GetDefaultName() const override { return TEXT("HistogramScan");}
	virtual FText							GetTooltipText() const override { return FText::FromString(TEXT("Lets you drive the contrast and position of the histogram. Input must be a grayscale image.")); } 
};

