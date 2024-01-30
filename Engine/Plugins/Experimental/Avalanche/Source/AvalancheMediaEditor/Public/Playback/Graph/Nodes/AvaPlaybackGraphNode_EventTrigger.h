// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackGraphNode.h"
#include "AvaPlaybackGraphNode_EventTrigger.generated.h"

UCLASS()
class AVALANCHEMEDIAEDITOR_API UAvaPlaybackGraphNode_EventTrigger : public UAvaPlaybackGraphNode
{
	GENERATED_BODY()

public:
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;
	virtual FName GetOutputPinCategory() const override;

	//UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	//~UEdGraphNode interface
};
