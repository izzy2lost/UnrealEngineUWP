// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_BlendStack.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "AnimGraphNode_MotionMatching.generated.h"


UCLASS(MinimalAPI, Experimental)
class UAnimGraphNode_MotionMatching : public UAnimGraphNode_BlendStack_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_MotionMatching Node;

	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	virtual UScriptStruct* GetTimePropertyStruct() const override;
	virtual void BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog) override;

	virtual FAnimNode_BlendStack_Standalone* GetBlendStackNode() const override { return (FAnimNode_BlendStack_Standalone*)(&Node); }
};
