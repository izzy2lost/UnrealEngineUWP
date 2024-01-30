// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Playback/Nodes/Events/Actions/AvalancheAnimations.h"
#include "AvaPlaybackNode_PlayAnim.generated.h"

UCLASS()
class AVALANCHEMEDIA_API UAvaPlaybackNode_PlayAnim : public UAvaPlaybackNodeAction
{
	GENERATED_BODY()

public:
	
	virtual FText GetNodeDisplayNameText() const override;
	virtual FText GetNodeTooltipText() const override;
	virtual void OnEventTriggered(const FAvaPlaybackEventParameters& InEventParameters) override;

	virtual void PreDryRun() override;
	virtual void DryRun(const TArray<UAvaPlaybackNode*>& InAncestors) override;
	virtual void PostDryRun() override;
protected:
	
	UPROPERTY(VisibleAnywhere, Category = "Motion Design")
	TMap<FSoftObjectPath, FAvalancheAnimations> AnimationMap;

	//All the Blueprints connected to this Node from the Dry Run (only populated while Dry Running!)
	TSet<FSoftObjectPath> SeenAssetsInDryRun;
};
