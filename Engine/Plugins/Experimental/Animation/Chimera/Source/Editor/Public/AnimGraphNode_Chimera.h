// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_BlendStack.h"
#include "Chimera/AnimNode_Chimera.h"
#include "AnimGraphNode_Chimera.generated.h"

UCLASS()
class CHIMERAEDITOR_API UAnimGraphNode_Chimera : public UAnimGraphNode_BlendStack_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_Chimera Node;

public:
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FAnimNode_BlendStack_Standalone* GetBlendStackNode() const override { return (FAnimNode_BlendStack_Standalone*)(&Node); }
};
