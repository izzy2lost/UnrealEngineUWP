// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTagHandle.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "UObject/Object.h"

#include "AvalanchePagePlayer.generated.h"

class FAvaMediaPlaybackInstance;
class UAvalanchePagePlayer;
class UAvalanchePlayback;
class UAvalanchePlaylist;
struct FAvalanchePage;

UENUM(BlueprintType)
enum class EAvaPlayType : uint8
{
	PlayFromStart,
	PreviewFromStart,
	PreviewFromFrame
};

/**
 *	To support combo templates, a page is now potentially composed of a number of
 *	playback instances that each have their own player.
 */
UCLASS()
class AVALANCHEMEDIA_API UAvaRundownPlaybackInstancePlayer : public UObject
{
	GENERATED_BODY()

public:
	UAvaRundownPlaybackInstancePlayer();
	virtual ~UAvaRundownPlaybackInstancePlayer() override;

	bool Load(const UAvalanchePagePlayer& InPagePlayer, const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, int32 InSubPageIndex, const FGuid& InInstanceId);
	bool IsLoaded() const;

	void Play(const UAvalanchePagePlayer& InPagePlayer, const UAvalanchePlaylist* InPlaylist, EAvaPlayType InPlayType, bool bInIsUsingTransitionLogic);
	bool IsPlaying() const;

	bool Continue(const FString& InChannelName);
	bool Stop();
	
	FAvaMediaPlaybackInstance* GetPlaybackInstance() const { return PlaybackInstance ? PlaybackInstance.Get() : nullptr; }

	FGuid GetPlaybackInstanceId() const { return PlaybackInstance ? PlaybackInstance->GetInstanceId() : FGuid(); }

	bool HasPlayable(const UAvalanchePlayable* InPlayable) const;

	UAvalanchePlayable* GetFirstPlayable() const;

	UAvalanchePagePlayer* GetPagePlayer() const;

	void SetPagePlayer(UAvalanchePagePlayer* InPagePlayer);

public:	
	UPROPERTY()
	FAvaTagHandle TransitionLayer;

	UPROPERTY()
	FSoftObjectPath SourceAssetPath;
	
	UPROPERTY()
	TObjectPtr<UAvalanchePlayback> Playback;

	TSharedPtr<FAvaMediaPlaybackInstance> PlaybackInstance;
};

UCLASS()
class AVALANCHEMEDIA_API UAvalanchePagePlayer : public UObject
{
	GENERATED_BODY()
	
public:
	UAvalanchePagePlayer();
	virtual ~UAvalanchePagePlayer() override;

	/** Initialize Page player without loading any playback instances. */
	bool Initialize(UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannel);

	/** Initialize the Page player and loads all playback instances. */
	bool InitializeAndLoad(UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, bool bInIsPreview, const FName& InPreviewChannel);

	/**
	 * Load an instance player for the "sub page" at the given index.
	 * The index is the template index in the combo template.
	 */
	UAvaRundownPlaybackInstancePlayer* LoadInstancePlayer(int32 InSubPageIndex, const FGuid& InInstanceId);

	/** Add a pre-existing instance player to this page player. */
	void AddInstancePlayer(UAvaRundownPlaybackInstancePlayer* InExistingInstancePlayer);

	/** Returns true if at least one of the instance player is loaded, false otherwise. */
	bool IsLoaded() const;
	
	bool Play(EAvaPlayType InPlayType, bool bInIsUsingTransitionLogic);

	/** Returns true if at least one of the instance player is playing, false otherwise. */
	bool IsPlaying() const;
	
	bool Continue();
	bool Stop();

	int32 GetNumInstancePlayers() const { return InstancePlayers.Num(); }

	FAvaMediaPlaybackInstance* GetPlaybackInstance(int32 InIndex = 0) const
	{
		return InstancePlayers.IsValidIndex(InIndex) && IsValid(InstancePlayers[InIndex]) ? InstancePlayers[InIndex]->GetPlaybackInstance() : nullptr;
	}

	FGuid GetPlaybackInstanceId(int32 InIndex = 0) const
	{
		return InstancePlayers.IsValidIndex(InIndex) && IsValid(InstancePlayers[InIndex]) ? InstancePlayers[InIndex]->GetPlaybackInstanceId() : FGuid();
	}

	UAvalanchePlayback* GetPlayback(int32 InIndex = 0) const
	{
		return InstancePlayers.IsValidIndex(InIndex) && IsValid(InstancePlayers[InIndex]) ? InstancePlayers[InIndex]->Playback : nullptr;
	}

	FSoftObjectPath GetSourceAssetPath(int32 InIndex = 0) const
	{
		return InstancePlayers.IsValidIndex(InIndex) && IsValid(InstancePlayers[InIndex]) ? InstancePlayers[InIndex]->SourceAssetPath : FSoftObjectPath();
	}

	UAvaRundownPlaybackInstancePlayer* GetInstancePlayer(int32 InIndex) const
	{
		return InstancePlayers.IsValidIndex(InIndex)? InstancePlayers[InIndex] : nullptr;
	}

	void ForEachInstancePlayer(TFunctionRef<void(UAvaRundownPlaybackInstancePlayer*)> InFunction)
	{
		for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
		{
			InFunction(InstancePlayer.Get());
		}
	}

	void ForEachInstancePlayer(TFunctionRef<void(const UAvaRundownPlaybackInstancePlayer*)> InFunction) const
	{
		for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InstancePlayers)
		{
			InFunction(InstancePlayer.Get());
		}
	}
	
	UAvalanchePlaylist* GetPlaylist() const { return PlaylistWeak.Get(); }

	static int32 GetPageIdFromInstanceUserData(const FString& InUserData);

	static void SetInstanceUserDataFromPage(FAvaMediaPlaybackInstance& InPlaybackInstance, const FAvalanchePage& InPage);

	bool HasPlayable(const UAvalanchePlayable* InPlayable) const;

	UAvaRundownPlaybackInstancePlayer* FindInstancePlayerForPlayable(const UAvalanchePlayable* InPlayable) const;

	UAvaRundownPlaybackInstancePlayer* FindInstancePlayerByInstanceId(const FGuid& InInstanceId) const;

	UAvaRundownPlaybackInstancePlayer* FindInstancePlayerByAssetPath(const FSoftObjectPath& InAssetPath) const;
	
protected:
	UAvaRundownPlaybackInstancePlayer* CreateAndLoadInstancePlayer(const UAvalanchePlaylist* InPlaylist, const FAvalanchePage& InPage, int32 InSubPageIndex, const FGuid& InInstanceId);

	void RemoveInstancePlayer(UAvaRundownPlaybackInstancePlayer* InInstancePlayer);
	
	void HandleOnPlayableSequenceEvent(UAvalanchePlayable* InPlayable, const FName& SequenceName, EAvalanchePlayableSequenceEventType InEventType);

public:
	UPROPERTY()
	int32 PageId = INDEX_NONE;

	UPROPERTY()
	bool bIsPreview = false;

	/** @remark For previews, the channel name will not be the one set in the page. */
	UPROPERTY()
	FName ChannelFName;

	/** @remark For previews, the channel name will not be the one set in the page. */
	UPROPERTY()
	FString ChannelName;

	UPROPERTY()
	TArray<TObjectPtr<UAvaRundownPlaybackInstancePlayer>> InstancePlayers;

	/** Instances that are excluded from the next transition. */
	UPROPERTY(Transient)
	TSet<FGuid> InstancesExcludedFromTransition;

protected:
	TWeakObjectPtr<UAvalanchePlaylist> PlaylistWeak;
};
