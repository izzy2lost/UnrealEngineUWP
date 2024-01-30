// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Playback/AvalanchePlayable.h"
#include "Playback/Transition/AvaMediaPlaybackTransition.h"
#include "UObject/Object.h"

#include "AvaMediaServerPlaybackTransition.generated.h"

class FAvaMediaPlaybackInstance;
class FAvaMediaPlaybackServer;
class UAvalanchePlayableTransition;

/**
 * Class for creating and tracking playback graph instance transitions on the server.
 * It is responsible for creating the playable transition object when requested from
 * the playback graphs.
 *
 * This class handles each playback graph instance as a single playable. It is
 * meant to be used by the playback server primarily.
 */
UCLASS()
class UAvaMediaServerPlaybackTransition : public UAvaMediaPlaybackTransition
{
	GENERATED_BODY()
	
public:
	void SetChannelName(const FName& InChannelName) { ChannelName = InChannelName; }
	void SetTransitionId(const FGuid& InTransitionId) { TransitionId = InTransitionId; }
	void SetClientName(const FString& InClientName) { ClientName = InClientName; }
	void SetUnloadDiscardedInstances(bool bInUnloadDiscardedInstances) { bUnloadDiscardedInstances = bInUnloadDiscardedInstances; }
	void SetTransitionFlags(EAvalanchePlayableTransitionFlags InTransitionFlags) { TransitionFlags = InTransitionFlags; }
	void SetEnterInstanceIds(const TArray<FGuid>& InInstanceIds) { EnterInstanceIds = InInstanceIds; }
	void SetEnterValues(const TArray<FAvalancheRemoteControlValues>& InEnterValues);
	bool AddEnterInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance);
	bool AddPlayingInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance);
	bool AddExitInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance);

	void TryResolveInstances(const FAvaMediaPlaybackServer& InPlaybackServer);
	
	//~ Begin IAvaPlayableVisibilityConstraint
	virtual bool IsVisibilityConstrained(const UAvalanchePlayable* InPlayable) const override;
	//~ End IAvaPlayableVisibilityConstraint
	
	//~ Begin UAvaMediaPlaybackTransition
	virtual bool CanStart(bool& bOutShouldDiscard) const override;
	virtual void Start() override;
	virtual void Stop() override;
	virtual bool IsRunning() const override;
	//~ End UAvaMediaPlaybackTransition

	/** Returns the channel this transition is happening in. A transition can only have instances within the same channel. */
	FName GetChannelName() const { return ChannelName; }

	FString GetPrettyTransitionInfo() const;
	FString GetBriefTransitionDescription() const;
	
protected:
	TSharedPtr<FAvaMediaPlaybackInstance> FindInstanceForPlayable(const UAvalanchePlayable* InPlayable);

	void OnTransitionEvent(UAvalanchePlayable* InPlayable, UAvalanchePlayableTransition* InTransition, EAvalanchePlayableTransitionEventFlags InTransitionFlags);
	void OnPlayableCreated(UAvalanchePlayback* InPlayback, UAvalanchePlayable* InPlayable);
	
	void MakePlayableTransition();

	void LogDetailedTransitionInfo() const;

	void RegisterToPlayableTransitionEvent();
	void UnregisterFromPlayableTransitionEvent() const;

	bool AddPlaybackInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance, TArray<TWeakPtr<FAvaMediaPlaybackInstance>>& OutPlaybackInstancesWeak);	
	void UpdateChannelName(const FAvaMediaPlaybackInstance* InPlaybackInstance);

protected:
	FString ClientName;
	FName ChannelName;
	FGuid TransitionId;
	bool bUnloadDiscardedInstances = false;
	EAvalanchePlayableTransitionFlags TransitionFlags = EAvalanchePlayableTransitionFlags::None;
	
	TArray<FGuid> EnterInstanceIds;

	TArray<TWeakPtr<FAvaMediaPlaybackInstance>> EnterPlaybackInstancesWeak;
	TArray<TWeakPtr<FAvaMediaPlaybackInstance>> PlayingPlaybackInstancesWeak;
	TArray<TWeakPtr<FAvaMediaPlaybackInstance>> ExitPlaybackInstancesWeak;
	TArray<TSharedPtr<FAvalancheRemoteControlValues>> EnterValues;
	
	UPROPERTY(Transient)
	TObjectPtr<UAvalanchePlayableTransition> PlayableTransition;
};