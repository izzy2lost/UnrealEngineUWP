// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AvaPlaybackNodePlayer.h"
#include "AvaPlaybackNodeBlueprintPlayer.generated.h"

class UAvalancheBlueprint;

UCLASS()
class AVALANCHEMEDIA_API UAvaPlaybackNodeBlueprintPlayer : public UAvaPlaybackNodePlayer
{
	GENERATED_BODY()

public:
	UAvaPlaybackNodeBlueprintPlayer();

	virtual void RefreshNode(bool bDryRunGraph) override;
	virtual void PostLoad() override;
	
	TSoftObjectPtr<UAvalancheBlueprint> GetAvalancheAsset() const { return BlueprintAsset; }
	void SetAvalancheAsset(const TSoftObjectPtr<UAvalancheBlueprint>& InAsset);
	
	void UpdateDisplayNameText();

	virtual const FSoftObjectPath& GetAvalancheAssetPath() const override { return BlueprintAsset.ToSoftObjectPath();}
	
	virtual FAvaSoftAssetPtr GetAvalancheAssetPtr() const override;
	
protected:
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	TSoftObjectPtr<UAvalancheBlueprint> BlueprintAsset;
};
