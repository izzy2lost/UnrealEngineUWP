// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Nodes/Events/AvaPlaybackNodeAction.h"
#include "AvaPlaybackNode_PreloadPlayer.generated.h"

UCLASS()
class AVALANCHEMEDIA_API UAvaPlaybackNode_PreloadPlayer : public UAvaPlaybackNodeAction
{
	GENERATED_BODY()

public:
	
	virtual FText GetNodeDisplayNameText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void OnEventTriggered(const FAvaPlaybackEventParameters& InEventParameters) override;
};
