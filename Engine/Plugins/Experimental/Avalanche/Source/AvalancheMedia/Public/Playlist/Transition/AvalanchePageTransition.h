// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTag.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/Transition/AvaMediaPlaybackTransition.h"
#include "UObject/Object.h"

#include "AvalanchePageTransition.generated.h"

class FAvaPlayableTransitionBuilder;
class UAvaRundownPlaybackInstancePlayer;
class UAvalanchePagePlayer;
class UAvalanchePlayableTransition;
class UAvalanchePlayback;
class UAvalanchePlaylist;
enum class EAvaMediaPlayableTransitionEntryRole : uint8;

/**
 * Class for creating and tracking page transitions in the playlist.
 * It is responsible for creating the playable transition object when requested from
 * the playback graphs.
 */
UCLASS()
class AVALANCHEMEDIA_API UAvalanchePageTransition : public UAvaMediaPlaybackTransition
{
	GENERATED_BODY()
	
public:
	bool AddEnterPage(UAvalanchePagePlayer* InPagePlayer);
	bool AddPlayingPage(UAvalanchePagePlayer* InPagePlayer);
	bool AddExitPage(UAvalanchePagePlayer* InPagePlayer);
	
	//~ Begin IAvaPlayableVisibilityConstraint
	virtual bool IsVisibilityConstrained(const UAvalanchePlayable* InPlayable) const override;
	//~ End IAvaPlayableVisibilityConstraint
	
	//~ Begin UAvaMediaPlaybackTransition
	virtual bool CanStart(bool& bOutShouldDiscard) const override;
	virtual void Start() override;
	virtual void Stop() override;
	virtual bool IsRunning() const override;
	//~ End UAvaMediaPlaybackTransition

	/** Returns the channel this transition is happening in. A transition can only have pages within the same channel. */
	FName GetChannelName() const { return ChannelName; }

	bool HasEnterPages() const { return !EnterPlayersWeak.IsEmpty(); }

	bool HasEnterPagesWithNoTransitionLogic() const;

	bool HasPagePlayer(const UAvalanchePagePlayer* InPagePlayer) const;

	bool ContainsTransitionLayer(const FAvaTagId& InTagId) const;

	UAvalanchePlaylist* GetPlaylist() const;

protected:
	UAvaRundownPlaybackInstancePlayer* FindInstancePlayerForPlayable(const UAvalanchePlayable* InPlayable) const;
	UAvalanchePagePlayer* FindPagePlayerForPlayable(const UAvalanchePlayable* InPlayable) const;

	void OnTransitionEvent(UAvalanchePlayable* InPlayable, UAvalanchePlayableTransition* InTransition, EAvalanchePlayableTransitionEventFlags InTransitionFlags);
	void OnPlayableCreated(UAvalanchePlayback* InPlayback, UAvalanchePlayable* InPlayable);

	void AddPlayersToBuilder(FAvaPlayableTransitionBuilder& InOutBuilder, const TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& InPlayersWeak, const TCHAR* InCategory, EAvaMediaPlayableTransitionEntryRole InEntryRole) const;
	void AddPlayablesToBuilder(FAvaPlayableTransitionBuilder& InOutBuilder, const UAvalanchePagePlayer* InPlayer, const TCHAR* InCategory, EAvaMediaPlayableTransitionEntryRole InEntryRole) const;

	void MakePlayableTransition();

	void LogDetailedTransitionInfo() const;
	FString GetBriefTransitionDescription() const;

	void RegisterToPlayableTransitionEvent();
	void UnregisterFromPlayableTransitionEvent() const;

	void RegisterEnterPagePlayerEvents(UAvalanchePagePlayer* InPagePlayer);
	void UnregisterEnterPagePlayerEvents(UAvalanchePagePlayer* InPagePlayer);

	bool AddPagePlayer(UAvalanchePagePlayer* InPagePlayer, TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& OutPagePlayersWeak);	
	void UpdateChannelName(const UAvalanchePagePlayer* InPagePlayer);

protected:
	FName ChannelName;
	
	TArray<TWeakObjectPtr<UAvalanchePagePlayer>> EnterPlayersWeak;
	TArray<TWeakObjectPtr<UAvalanchePagePlayer>> PlayingPlayersWeak;
	TArray<TWeakObjectPtr<UAvalanchePagePlayer>> ExitPlayersWeak;

	TSet<FAvaTagId> CachedTransitionLayers;
	
	UPROPERTY(Transient)
	TObjectPtr<UAvalanchePlayableTransition> PlayableTransition;
};