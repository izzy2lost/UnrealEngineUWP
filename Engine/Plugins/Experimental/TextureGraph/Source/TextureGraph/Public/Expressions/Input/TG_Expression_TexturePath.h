// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TG_Expression_InputParam.h"
#include "Engine/Texture.h"
#include "TG_Texture.h"

#include "TG_Expression_TexturePath.generated.h"

UCLASS()
class TEXTUREGRAPH_API UTG_Expression_TexturePath : public UTG_Expression_InputParam
{
	GENERATED_BODY()

public:

	virtual void Evaluate(FTG_EvaluationContext* InContext) override;
	virtual bool Validate(MixUpdateCyclePtr	Cycle) override;
	
	// The output of the node, which is the loaded texture from the path
	UPROPERTY(meta = (TGType = "TG_Output", PinDisplayName = "", HideInnerPropertiesInNode))
	FTG_Texture Output;

	// Input file path of the texture
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = NoCategory, meta = (TGType = "TG_Input", TGPinNotConnectable, NoResetToDefault) )
	FString Path;

	// The input texture that was loaded from the path
	UPROPERTY(meta = (TGType = "TG_InputParam"))
	FTG_Texture Texture;

	class ULayerChannel* Channel;
	virtual FTG_Name GetDefaultName() const override { return TEXT("TexturePath"); }
	virtual void SetTitleName(FName NewName) override;
	virtual FName GetTitleName() const override;
	virtual FText GetTooltipText() const override { return FText::FromString(TEXT("Loads a texture from a path.")); }
};

