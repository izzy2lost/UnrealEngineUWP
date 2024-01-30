// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackGraphNode.h"
#include "AvaPlaybackGraphNode_Root.generated.h"

UCLASS()
class AVALANCHEMEDIAEDITOR_API UAvaPlaybackGraphNode_Root : public UAvaPlaybackGraphNode
{
	GENERATED_BODY()
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;

	//UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;
	virtual TSharedPtr<SGraphNode> CreateVisualWidget() override;
	//~UEdGraphNode interface
};
