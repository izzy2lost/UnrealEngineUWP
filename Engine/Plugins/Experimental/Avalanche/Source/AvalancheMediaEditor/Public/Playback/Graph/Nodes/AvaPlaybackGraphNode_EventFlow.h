// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackGraphNode.h"
#include "AvaPlaybackGraphNode_EventFlow.generated.h"

UCLASS()
class AVALANCHEMEDIAEDITOR_API UAvaPlaybackGraphNode_EventFlow : public UAvaPlaybackGraphNode
{
	GENERATED_BODY()

public:
	
	virtual TSubclassOf<UAvaPlaybackNode> GetPlaybackNodeClass() const override;

	virtual FName GetInputPinCategory(int32 InputPinIndex) const override;
	virtual FName GetOutputPinCategory() const override;
};
