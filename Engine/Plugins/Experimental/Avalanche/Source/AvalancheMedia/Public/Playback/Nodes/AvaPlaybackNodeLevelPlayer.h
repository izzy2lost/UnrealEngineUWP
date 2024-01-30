// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AvaPlaybackNodePlayer.h"
#include "AvaPlaybackNodeLevelPlayer.generated.h"

class UWorld;

UCLASS()
class AVALANCHEMEDIA_API UAvaPlaybackNodeLevelPlayer : public UAvaPlaybackNodePlayer
{
	GENERATED_BODY()

public:
	
	UAvaPlaybackNodeLevelPlayer();
	
	virtual void RefreshNode(bool bDryRunGraph) override;
	virtual void PostLoad() override;
	
	TSoftObjectPtr<UWorld> GetAvalancheAsset() const { return LevelAsset; }
	void SetAvalancheAsset(const TSoftObjectPtr<UWorld>& InAsset);
	
	void UpdateDisplayNameText();

	virtual const FSoftObjectPath& GetAvalancheAssetPath() const override { return LevelAsset.ToSoftObjectPath();}

	virtual FAvaSoftAssetPtr GetAvalancheAssetPtr() const override;

protected:
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	TSoftObjectPtr<UWorld> LevelAsset;
};

