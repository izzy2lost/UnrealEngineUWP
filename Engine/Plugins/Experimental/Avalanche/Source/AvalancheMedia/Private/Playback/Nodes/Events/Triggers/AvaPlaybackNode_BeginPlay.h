// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeTrigger.h"
#include "AvaPlaybackNode_BeginPlay.generated.h"


UCLASS()
class AVALANCHEMEDIA_API UAvaPlaybackNode_BeginPlay : public UAvaPlaybackNodeTrigger
{
	GENERATED_BODY()

public:

	virtual FText GetNodeDisplayNameText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void NotifyPlaybackStateChanged(bool bPlaying) override;
	
};
