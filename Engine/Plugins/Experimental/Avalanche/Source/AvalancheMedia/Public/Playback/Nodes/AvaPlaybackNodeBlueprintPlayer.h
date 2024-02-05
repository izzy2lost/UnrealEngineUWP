// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaPlaybackNodePlayer.h"
#include "CoreMinimal.h"
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
	
	TSoftObjectPtr<UAvalancheBlueprint> GetAsset() const { return Asset; }
	void SetAsset(const TSoftObjectPtr<UAvalancheBlueprint>& InAsset);
	
	void UpdateDisplayNameText();

	virtual const FSoftObjectPath& GetAssetPath() const override { return Asset.ToSoftObjectPath();}
	
	virtual FAvaSoftAssetPtr GetAssetPtr() const override;
	
protected:
	
	UPROPERTY(EditAnywhere, Category = "Motion Design")
	TSoftObjectPtr<UAvalancheBlueprint> Asset;
};
