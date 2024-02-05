// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "Playable/AvaPlayable.h"
#include "AvaPlayableBlueprint.generated.h"

UCLASS(NotBlueprintable, BlueprintType, ClassGroup = "Motion Design Playable", 
	meta = (DisplayName = "Motion Design Blueprint Playable"))
class UAvaPlayableBlueprint : public UAvaPlayable
{
	GENERATED_BODY()

public:
	//~ Begin UAvaPlayable
	virtual bool LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, bool bInInitiallyVisible) override;
	virtual bool UnloadAsset() override;
	virtual const FSoftObjectPath& GetSourceAssetPath() const override;
	virtual EAvaPlayableStatus GetPlayableStatus() const override;
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	virtual EAvaPlayableCommandResult ExecuteAnimationCommand(EAvaPlaybackAnimAction InAnimAction, const FAvaPlaybackAnimPlaySettings& InAnimPlaySettings) override;

protected:
	virtual bool InitPlayable(const FPlayableCreationInfo& InPlayableInfo) override;
	virtual void OnPlay() override;
	virtual void OnEndPlay() override;
	//~ End UAvaPlayable

public:
	UAvalancheBlueprint* GetManagedBlueprint() const { return ManagedBlueprint; }
	
protected:
	bool LoadBlueprintInternal(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceBlueprint);

protected:
	UPROPERTY(Transient)
	TSoftObjectPtr<UAvalancheBlueprint> SourceBlueprint;

	/*
	 * The Managed Blueprint, a Duplication of the Source Blueprint when Begin Play was called.
	 * This shouldn't be GCd while in play, as it contains Animation and other data used during play.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAvalancheBlueprint> ManagedBlueprint;
};