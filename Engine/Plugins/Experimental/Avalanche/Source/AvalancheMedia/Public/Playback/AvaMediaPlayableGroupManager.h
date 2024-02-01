// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

#include "AvaMediaPlayableGroupManager.generated.h"

class UAvaMediaPlayableGroup;
class UAvaMediaPlayableGroupManager;
class UAvaGameInstance;

/**
 * Manager for the shared playable groups per channel.
 */
UCLASS()
class AVALANCHEMEDIA_API UAvaMediaChannelPlayableGroupManager : public UObject
{
	GENERATED_BODY()

public:
	UAvaMediaPlayableGroup* GetOrCreateSharedLevelGroup(bool bInIsRemoteProxy);

	UAvaMediaPlayableGroupManager* GetPlayableGroupManager() const;
	
protected:
	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	void Shutdown();
	
protected:
	FName ChannelName;

	/**
	 * This is the shared playable group for this channel.
	 * 
	 * For now all the levels go in the same group for a given channel.
	 * This a weak ptr because it is only a cache. The ownership
	 * of the group is with the playable objects.
	 */
	TWeakObjectPtr<UAvaMediaPlayableGroup> SharedLevelGroupWeak;

	/**
	 * For remote proxy playables, we need a specialized group
	 * that doesn't implement the local game instance but still
	 * provides the correct logic to emulate functionality.
	 */
	TWeakObjectPtr<UAvaMediaPlayableGroup> SharedRemoteProxyLevelGroupWeak;

	friend class UAvaMediaPlayableGroupManager;
};

/**
 * Manager for the shared playable groups.
 * The scope of this manager is either global (in the global playback manager)
 * or for a given playback manager.
 */
UCLASS()
class AVALANCHEMEDIA_API UAvaMediaPlayableGroupManager : public UObject
{
	GENERATED_BODY()
	
public:
	void Init();

	void Shutdown();

	void Tick(double InDeltaSeconds);

	UAvaMediaChannelPlayableGroupManager* FindChannelManager(const FName& InChannelName) const
	{
		const TObjectPtr<UAvaMediaChannelPlayableGroupManager>* ChannelManager = ChannelManagers.Find(InChannelName);
		return ChannelManager ? *ChannelManager : nullptr;
	}

	UAvaMediaChannelPlayableGroupManager* FindOrAddChannelManager(const FName& InChannelName);

	UAvaMediaPlayableGroup* GetOrCreateSharedLevelGroup(const FName& InChannelName, bool bInIsRemoteProxy)
	{
		UAvaMediaChannelPlayableGroupManager* ChannelManager = FindOrAddChannelManager(InChannelName);
		return ChannelManager ? ChannelManager->GetOrCreateSharedLevelGroup(bInIsRemoteProxy) : nullptr;
	}

	void RegisterForLevelStreamingUpdate(UAvaMediaPlayableGroup* InPlayableGroup);
	void UnregisterFromLevelStreamingUpdate(UAvaMediaPlayableGroup* InPlayableGroup);

	void RegisterForTransitionTicking(UAvaMediaPlayableGroup* InPlayableGroup);
	void UnregisterFromTransitionTicking(UAvaMediaPlayableGroup* InPlayableGroup);

protected:
	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	void OnGameInstanceEndPlay(UAvaGameInstance* InGameInstance, FName InChannelName);

	void UpdateLevelStreaming();
	
	void TickTransitions(double InDeltaSeconds);
	
protected:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAvaMediaChannelPlayableGroupManager>> ChannelManagers;

	bool bIsUpdatingStreaming = false;
	TSet<TWeakObjectPtr<UAvaMediaPlayableGroup>> GroupsToUpdateStreaming;

	bool bIsTickingTransitions = false;
	TSet<TWeakObjectPtr<UAvaMediaPlayableGroup>> GroupsToTickTransitions;
};