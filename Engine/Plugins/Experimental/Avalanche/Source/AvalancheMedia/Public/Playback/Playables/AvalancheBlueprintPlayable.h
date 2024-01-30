// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Playback/AvalanchePlayable.h"

#include "AvalancheBlueprintPlayable.generated.h"

UCLASS(NotBlueprintable, BlueprintType)
class AVALANCHEMEDIA_API UAvalancheBlueprintPlayable : public UAvalanchePlayable
{
	GENERATED_BODY()

public:
	//~ Begin UAvalanchePlayable
	virtual bool LoadAsset(const FAvaSoftAssetPtr& InAvalancheSourceAsset, bool bInInitiallyVisible) override;
	virtual bool UnloadAsset() override;
	virtual const FSoftObjectPath& GetSourceAssetPath() const override;
	virtual EAvalanchePlayableStatus GetPlayableStatus() const override;
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	virtual EAvalanchePlayableCommandResult ExecuteAnimationCommand(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings) override;

protected:
	virtual bool InitPlayable(const FPlayableCreationInfo& InPlayableInfo) override;
	virtual void OnPlay() override;
	virtual void OnEndPlay() override;
	//~ End UAvalanchePlayable

public:
	UAvalancheBlueprint* GetManagedAvalancheBlueprint() const { return ManagedAvalancheBlueprint; }
	
protected:
	bool LoadAvalancheBlueprintInternal(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceAvalancheBlueprint);

protected:
	UPROPERTY(Transient)
	TSoftObjectPtr<UAvalancheBlueprint> SourceAvalancheBlueprint;

	/*
	 * The Managed Avalanche, a Duplication of the Source Avalanche when Begin Play was called.
	 * This shouldn't be GCd while in play, as it contains Animation and other data used during play.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAvalancheBlueprint> ManagedAvalancheBlueprint;
};