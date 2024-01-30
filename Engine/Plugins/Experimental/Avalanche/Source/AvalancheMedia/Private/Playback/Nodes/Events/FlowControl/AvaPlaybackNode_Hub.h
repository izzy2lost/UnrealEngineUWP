// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeFlow.h"
#include "AvaPlaybackNode_Hub.generated.h"

UCLASS()
class AVALANCHEMEDIA_API UAvaPlaybackNode_Hub : public UAvaPlaybackNodeFlow
{
	GENERATED_BODY()

public:
	
	virtual FText GetNodeDisplayNameText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual int32 GetMaxChildNodes() const override { return MaxAllowedChildNodes; }

	virtual void TickEvent(float DeltaTime, FAvaPlaybackEventParameters& OutEventParameters) override;
};
