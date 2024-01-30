// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackGraphNode.h"
#include "AvaPlaybackGraphNode_BlueprintPlayer.generated.h"

UCLASS()
class AVALANCHEMEDIAEDITOR_API UAvaPlaybackGraphNode_BlueprintPlayer : public UAvaPlaybackGraphNode
{
	GENERATED_BODY()
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;
	virtual FName GetInputPinCategory(int32 InputPinIndex) const override;
	
	//UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	//~UEdGraphNode interface
};
