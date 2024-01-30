// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playback/AvalanchePlayable.h"

#include "AvalancheRemoteProxyPlayable.generated.h"

class IAvaMediaPlaybackClient;
namespace UE::AvaMediaPlaybackClient::Delegates
{
	struct FPlaybackSequenceEventArgs;
}

UCLASS(NotBlueprintable, BlueprintType)
class AVALANCHEMEDIA_API UAvalancheRemoteProxyPlayable : public UAvalanchePlayable
{
	GENERATED_BODY()
public:
	FName GetPlayingChannelFName() const { return PlayingChannelFName; }
	
	//~ Begin UAvalanchePlayable
	virtual bool LoadAsset(const FAvaSoftAssetPtr& InAvalancheSourceAsset, bool bInInitiallyVisible) override;
	virtual bool UnloadAsset() override;
	virtual const FSoftObjectPath& GetSourceAssetPath() const override { return SourceAssetPath; }
	virtual EAvalanchePlayableStatus GetPlayableStatus() const override;
	virtual IAvaSceneInterface* GetSceneInterface() const override;
	virtual EAvalanchePlayableCommandResult ExecuteAnimationCommand(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings) override;
	virtual EAvalanchePlayableCommandResult UpdateRemoteControlCommand(const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues) override;
	virtual bool ApplyCamera() override;
	virtual bool IsRemoteProxy() const override { return true; }
	virtual void SetUserData(const FString& InUserData) override;

protected:
	virtual bool InitPlayable(const FPlayableCreationInfo& InPlayableInfo) override;
	virtual void OnPlay() override;
	virtual void OnEndPlay() override;
	//~ End UAvalanchePlayable

	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	void RegisterClientEventHandlers();
	void UnregisterClientEventHandlers() const;

	static TArray<FString> GetOnlineServerForChannel(const FName& InChannelName);
	
	void HandleAvaMediaPlaybackSequenceEvent(IAvaMediaPlaybackClient& InPlaybackClient,
		const UE::AvaMediaPlaybackClient::Delegates::FPlaybackSequenceEventArgs& InEventArgs);
	
protected:
	/** Channel name this playable is playing on. */
	FName PlayingChannelFName;
	FString PlayingChannelName;

	FSoftObjectPath SourceAssetPath;
};
