// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackNodePlayer.h"
#include "CoreMinimal.h"
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
	
	TSoftObjectPtr<UWorld> GetAsset() const { return LevelAsset; }
	void SetAsset(const TSoftObjectPtr<UWorld>& InAsset);
	
	void UpdateDisplayNameText();

	virtual const FSoftObjectPath& GetAssetPath() const override { return LevelAsset.ToSoftObjectPath();}

	virtual FAvaSoftAssetPtr GetAssetPtr() const override;

protected:
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	TSoftObjectPtr<UWorld> LevelAsset;
};

