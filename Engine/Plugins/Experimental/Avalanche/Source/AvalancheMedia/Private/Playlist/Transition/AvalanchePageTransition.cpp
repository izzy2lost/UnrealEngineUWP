// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playlist/Transition/AvalanchePageTransition.h"

#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playback/Transition/AvalanchePlayableTransition.h"
#include "Playlist/AvalanchePagePlayer.h"
#include "Playlist/AvalanchePlaylist.h"

namespace UE::AvaMedia::Rundown::PageTransition::Private
{
	UAvalanchePlayable* GetPlayable(const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
	{
		return InInstancePlayer ? InInstancePlayer->GetFirstPlayable() : nullptr;
	}
	
	UAvalanchePlayable* GetPlayable(const UAvalanchePagePlayer* InPagePlayer, int32 InIndex)
	{
		return InPagePlayer ? GetPlayable(InPagePlayer->GetInstancePlayer(InIndex)) : nullptr;
	}

	UAvalanchePlayback* GetPlayback(const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
	{
		return InInstancePlayer ? InInstancePlayer->Playback : nullptr; 
	}
	
	UAvalanchePlayback* GetPlayback(const UAvalanchePagePlayer* InPagePlayer, int32 InIndex)
	{
		return InPagePlayer ? GetPlayback(InPagePlayer->GetInstancePlayer(InIndex)) : nullptr;
	}

	TSharedPtr<FAvalancheRemoteControlValues> GetRemoteControlValues(const UAvalanchePagePlayer* InPagePlayer)
	{
		if (UAvalanchePlaylist* Playlist = InPagePlayer->GetPlaylist())
		{
			const FAvalanchePage& Page = Playlist->GetPage(InPagePlayer->PageId);
			if (Page.IsValidPage())
			{
				return MakeShared<FAvalancheRemoteControlValues>(Page.GetRemoteControlValues());
			}
		}
		return nullptr;
	}

	FString GetPrettyPageInfo(const UAvalanchePagePlayer* InPagePlayer)
	{
		FString Info;
		Info += FString(TEXT("Channel: ")) + InPagePlayer->ChannelName;
		InPagePlayer->ForEachInstancePlayer([&Info](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
		{
			Info += FString(TEXT(", Instance Id: ")) + InInstancePlayer->GetPlaybackInstanceId().ToString();
			Info += FString(TEXT(", Asset: ")) + InInstancePlayer->SourceAssetPath.ToString();
		});
		return Info;
	}
	
	UAvaRundownPlaybackInstancePlayer* FindInstancePlayerForPlayable(const UAvalanchePlayable* InPlayable, const TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& InPagePlayersWeak)
	{
		for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : InPagePlayersWeak)
		{
			if (const UAvalanchePagePlayer* PagePlayer = PlayerWeak.Get())
			{
				if (UAvaRundownPlaybackInstancePlayer* InstancePlayer = PagePlayer->FindInstancePlayerForPlayable(InPlayable))
				{
					return InstancePlayer;
				}
			}
		}
		return nullptr;
	}

	UAvalanchePagePlayer* FindPagePlayerForPlayable(const UAvalanchePlayable* InPlayable, const TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& InPagePlayersWeak)
	{
		for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : InPagePlayersWeak)
		{
			UAvalanchePagePlayer* PagePlayer = PlayerWeak.Get();
			if (PagePlayer && PagePlayer->HasPlayable(InPlayable))
			{
				return PagePlayer;
			}
		}
		return nullptr;
	}
}

bool UAvalanchePageTransition::AddEnterPage(UAvalanchePagePlayer* InPagePlayer)
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;

	if (!InPagePlayer)
	{
		return false;	
	}

	// Multi-page constraint: Prevent pages on the same layer
	for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvalanchePagePlayer* Player = PlayerWeak.Get())
		{
			bool bHasLayer = false;
			InPagePlayer->ForEachInstancePlayer([this, &bHasLayer](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
			{
				if (CachedTransitionLayers.Contains(InInstancePlayer->TransitionLayer.TagId))
				{
					bHasLayer = true;
				}
			});
			
			if (bHasLayer)
			{
				UE_LOG(LogAvaPlaylist, Error,
						TEXT("Page Transition \"%s\" Error: page %d can't be played with page %d because they are on the same layer."),
						*GetFullName(), InPagePlayer->PageId, Player->PageId);
				return false;
			}
		}
	}

	RegisterEnterPagePlayerEvents(InPagePlayer);
	
	return AddPagePlayer(InPagePlayer, EnterPlayersWeak);
}

bool UAvalanchePageTransition::AddPlayingPage(UAvalanchePagePlayer* InPagePlayer)
{
	return AddPagePlayer(InPagePlayer, PlayingPlayersWeak);
}

bool UAvalanchePageTransition::AddExitPage(UAvalanchePagePlayer* InPagePlayer)
{
	return AddPagePlayer(InPagePlayer, ExitPlayersWeak);
}

bool UAvalanchePageTransition::IsVisibilityConstrained(const UAvalanchePlayable* InPlayable) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;
	bool bAllPlayablesLoaded = true;
	bool bIsPlayableInThisTransition = false;
	
	for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvalanchePagePlayer* Player = PlayerWeak.Get())
		{
			Player->ForEachInstancePlayer([InPlayable, &bIsPlayableInThisTransition, &bAllPlayablesLoaded](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
			{
				if (const UAvalanchePlayable* Playable = GetPlayable(InInstancePlayer))
				{
					if (Playable == InPlayable)
					{
						bIsPlayableInThisTransition = true;
					}
					const EAvalanchePlayableStatus PlayableStatus = Playable->GetPlayableStatus();
					if (PlayableStatus != EAvalanchePlayableStatus::Loaded && PlayableStatus != EAvalanchePlayableStatus::Visible)
					{
						bAllPlayablesLoaded = false;
					}
				}
			});
		}
	}
	
	return bIsPlayableInThisTransition && !bAllPlayablesLoaded;
}

bool UAvalanchePageTransition::CanStart(bool& bOutShouldDiscard) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;
	for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvalanchePagePlayer* Player = PlayerWeak.Get())
		{
			const int32 NumInstancePlayers = Player->GetNumInstancePlayers();
			for (int32 InstancePlayerIndex = 0; InstancePlayerIndex < NumInstancePlayers; ++InstancePlayerIndex)
			{
				if (const UAvalanchePlayable* Playable = GetPlayable(Player, InstancePlayerIndex))
				{
					const EAvalanchePlayableStatus PlayableStatus = Playable->GetPlayableStatus();

					if (PlayableStatus == EAvalanchePlayableStatus::Unknown
						|| PlayableStatus == EAvalanchePlayableStatus::Error)
					{
						// Discard the command
						bOutShouldDiscard = true;
						return false;
					}

					// todo: this might cause commands to become stale and fill the pending command list
					if (PlayableStatus == EAvalanchePlayableStatus::Unloaded)
					{
						return false;
					}

					// Asset status must be visible to run the command locally.
					// If not visible, the components are not yet added to the world.
					// For remote proxies, we can run the command immediately, it will wait for the asset to be visible on the server instead. 
					if (!Playable->IsRemoteProxy() && PlayableStatus != EAvalanchePlayableStatus::Visible)
					{
						// Keep the command in the queue for next tick.
						bOutShouldDiscard = false;
						return false;
					}
				}
				else
				{
					// Playables not created - Keep the command in the queue for next tick.
					bOutShouldDiscard = false;
					return false;
				}
			}
		}
		else
		{
			bOutShouldDiscard = true;
			return false;
		}
	}
	
	bOutShouldDiscard = true;
	return true;
}

void UAvalanchePageTransition::Start()
{
	RegisterToPlayableTransitionEvent();

	// May fail if playables are not loaded yet. Playables are loaded
	// when the playback object has ticked at least one.
	MakePlayableTransition();

	bool bTransitionStarted = false;

	if (PlayableTransition)
	{
		LogDetailedTransitionInfo();

		// Todo: validate the level streaming playables are finished streaming the asset.
		// Otherwise, transition start must be queued on playable streaming events. 
		bTransitionStarted = PlayableTransition->Start();
	}
	
	if (!bTransitionStarted)
	{
		Stop();
	}
}

void UAvalanchePageTransition::Stop()
{
	if (PlayableTransition)
	{
		PlayableTransition->Stop();
		PlayableTransition = nullptr;
	}

	for (TWeakObjectPtr<UAvalanchePagePlayer>& PagePlayerWeak : EnterPlayersWeak)
	{
		if (UAvalanchePagePlayer* PagePlayer = PagePlayerWeak.Get())
		{
			PagePlayer->InstancesExcludedFromTransition.Reset();
			UnregisterEnterPagePlayerEvents(PagePlayer);
		}
	}

	UnregisterFromPlayableTransitionEvent();

	if (UAvalanchePlaylist* Playlist = GetPlaylist())
	{
		Playlist->RemovePageTransition(this);
		Playlist->RemoveStoppedPagePlayers();
	}
	else
	{
		UE_LOG(LogAvaPlaylist, Error, TEXT("Page Transition \"%s\" Failed to remove transition: No rundown specified."), *GetFullName());
	}
}

bool UAvalanchePageTransition::IsRunning() const
{
	return 	PlayableTransition ? PlayableTransition->IsRunning() : false;
}

bool UAvalanchePageTransition::HasEnterPagesWithNoTransitionLogic() const
{
	const UAvalanchePlaylist* Playlist = GetPlaylist();
	if (!Playlist)
	{
		return false;
	}
	
	for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : EnterPlayersWeak)
	{
		if (const UAvalanchePagePlayer* Player = PlayerWeak.Get())
		{
			// Don't rely on instance players at this point. Assets may not be loaded.
			const FAvalanchePage& Page = Playlist->GetPage(Player->PageId);
			if (Page.IsValidPage() && !Page.HasTransitionLogic(Playlist))
			{
				return true;
			}
		}
	}
	return false;
}

bool UAvalanchePageTransition::HasPagePlayer(const UAvalanchePagePlayer* InPagePlayer) const
{
	return EnterPlayersWeak.Contains(InPagePlayer) || PlayingPlayersWeak.Contains(InPagePlayer) || ExitPlayersWeak.Contains(InPagePlayer);
}

bool UAvalanchePageTransition::ContainsTransitionLayer(const FAvaTagId& InTagId) const
{
	return CachedTransitionLayers.Contains(InTagId);
}

UAvalanchePlaylist* UAvalanchePageTransition::GetPlaylist() const
{
	return Cast<UAvalanchePlaylist>(GetOuter());
}

UAvaRundownPlaybackInstancePlayer* UAvalanchePageTransition::FindInstancePlayerForPlayable(const UAvalanchePlayable* InPlayable) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition;
	if (!InPlayable)
	{
		return nullptr;
	}
	
	if (UAvaRundownPlaybackInstancePlayer* FoundInstancePlayer = Private::FindInstancePlayerForPlayable(InPlayable, EnterPlayersWeak))
	{
		return FoundInstancePlayer;
	}
	if (UAvaRundownPlaybackInstancePlayer* FoundInstancePlayer = Private::FindInstancePlayerForPlayable(InPlayable, PlayingPlayersWeak))
	{
		return FoundInstancePlayer;
	}
	if (UAvaRundownPlaybackInstancePlayer* FoundInstancePlayer = Private::FindInstancePlayerForPlayable(InPlayable, ExitPlayersWeak))
	{
		return FoundInstancePlayer;
	}
	return nullptr;
}

UAvalanchePagePlayer* UAvalanchePageTransition::FindPagePlayerForPlayable(const UAvalanchePlayable* InPlayable) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition;
	if (UAvalanchePagePlayer* FoundPagePlayer = Private::FindPagePlayerForPlayable(InPlayable, EnterPlayersWeak))
	{
		return FoundPagePlayer;
	}
	if (UAvalanchePagePlayer* FoundPagePlayer = Private::FindPagePlayerForPlayable(InPlayable, PlayingPlayersWeak))
	{
		return FoundPagePlayer;
	}
	if (UAvalanchePagePlayer* FoundPagePlayer = Private::FindPagePlayerForPlayable(InPlayable, ExitPlayersWeak))
	{
		return FoundPagePlayer;
	}
	return nullptr;
}

void UAvalanchePageTransition::OnTransitionEvent(UAvalanchePlayable* InPlayable, UAvalanchePlayableTransition* InTransition, EAvalanchePlayableTransitionEventFlags InTransitionFlags)
{
	using namespace UE::AvaMediaPlayback::Utils;

	// not this transition.
	if (InTransition != PlayableTransition || PlayableTransition == nullptr)
	{
		return;
	}

	// Find the page player for this playable
	UAvaRundownPlaybackInstancePlayer* InstancePlayer = FindInstancePlayerForPlayable(InPlayable);
	UAvalanchePagePlayer* PagePlayer = InstancePlayer ? InstancePlayer->GetPagePlayer() : nullptr;

	if (InstancePlayer && EnumHasAnyFlags(InTransitionFlags, EAvalanchePlayableTransitionEventFlags::MarkPlayableDiscard))
	{
		UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Instance Player Marked for Discard: Id:%s, Asset:\"%s\""),
			*GetBriefFrameInfo(), *InstancePlayer->GetPlaybackInstanceId().ToString(), *InstancePlayer->SourceAssetPath.ToString());
	}
	
	if (InstancePlayer && EnumHasAnyFlags(InTransitionFlags, EAvalanchePlayableTransitionEventFlags::StopPlayable))
	{
		// Validating that we are not removing an "enter" playable.
		if (PlayableTransition->IsEnterPlayable(InPlayable))
		{
			UE_LOG(LogAvaPlaylist, Error,
				TEXT("Page Transition \"%s\" Error: An \"enter\" playable is being discarded for page %d."),
				*GetFullName(), (PagePlayer ? PagePlayer->PageId : -1));
		}

		UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Stopping Instance Player: Id:%s, Asset:\"%s\""),
			*GetBriefFrameInfo(), *InstancePlayer->GetPlaybackInstanceId().ToString(), *InstancePlayer->SourceAssetPath.ToString());

		// With combo-templates, page players can be partially stopped.
		InstancePlayer->Stop();

		// Check if the page player is still playing (i.e. if all instance players have been stopped)
		if (PagePlayer && !PagePlayer->IsPlaying())
		{
			UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Stopping Page Player: PageId:%d"), *GetBriefFrameInfo(), PagePlayer->PageId);

			// Stop the whole page player and propagate page events.
			PagePlayer->Stop();

			if (UAvalanchePlaylist* Playlist = PagePlayer->GetPlaylist())
			{
				Playlist->RemoveStoppedPagePlayers();
			}
			else
			{
				UE_LOG(LogAvaPlaylist, Error, TEXT("Page Transition \"%s\" failed to remove stopped players: No rundown specified."), *GetFullName());
			}
		}
	}
	
	if (EnumHasAnyFlags(InTransitionFlags, EAvalanchePlayableTransitionEventFlags::Finished))
	{
		using namespace UE::AvaMediaPlayback::Utils;
		UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Finishing Page Transition: %s"), *GetBriefFrameInfo(), *GetBriefTransitionDescription());

		Stop();
	}
}

void UAvalanchePageTransition::OnPlayableCreated(UAvalanchePlayback* InPlayback, UAvalanchePlayable* InPlayable)
{
	if (UAvaMediaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
	{
		PlayableGroup->RegisterVisibilityConstraint(this);
	}
}

void UAvalanchePageTransition::AddPlayersToBuilder(
	FAvaPlayableTransitionBuilder& InOutBuilder, const TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& InPlayersWeak, const TCHAR* InCategory, EAvaMediaPlayableTransitionEntryRole InEntryRole) const
{
	for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : InPlayersWeak)
	{
		if (UAvalanchePagePlayer* Player = PlayerWeak.Get())
		{
			AddPlayablesToBuilder(InOutBuilder, Player, InCategory, InEntryRole);
			
			// Excluded instances are only for current transition.
			// Todo: Ideally, it would be stored in the transition itself to avoid the clean up.
			Player->InstancesExcludedFromTransition.Reset();
		}
	}
}

void UAvalanchePageTransition::AddPlayablesToBuilder(FAvaPlayableTransitionBuilder& InOutBuilder, const UAvalanchePagePlayer* InPlayer, const TCHAR* InCategory, EAvaMediaPlayableTransitionEntryRole InEntryRole) const
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;

	for (const TObjectPtr<UAvaRundownPlaybackInstancePlayer>& InstancePlayer : InPlayer->InstancePlayers)
	{
		if (InPlayer->InstancesExcludedFromTransition.Contains(InstancePlayer->GetPlaybackInstanceId()))
		{
			continue;
		}
	
		if (UAvalanchePlayable* Playable = GetPlayable(InstancePlayer))
		{
			const bool bPlayableAdded = InOutBuilder.AddPlayable(Playable, InEntryRole);
			if (InEntryRole == EAvaMediaPlayableTransitionEntryRole::Enter && bPlayableAdded)
			{
				InOutBuilder.AddEnterPlayableValues(GetRemoteControlValues(InPlayer));
			}
		}
		else
		{
			// If this happens, likely the playable is not yet loaded.
			UE_LOG(LogAvaPlaylist, Error,
				TEXT("Page Transition \"%s\" Error: Failed to retrieve \"%s\" playable for page %d."),
				*GetFullName(), InCategory, InPlayer->PageId);
		}
	}
}

void UAvalanchePageTransition::MakePlayableTransition()
{
	FAvaPlayableTransitionBuilder TransitionBuilder;
	AddPlayersToBuilder(TransitionBuilder, EnterPlayersWeak, TEXT("Enter"), EAvaMediaPlayableTransitionEntryRole::Enter);
	AddPlayersToBuilder(TransitionBuilder, PlayingPlayersWeak, TEXT("Playing"), EAvaMediaPlayableTransitionEntryRole::Playing);
	AddPlayersToBuilder(TransitionBuilder, ExitPlayersWeak, TEXT("Exit"), EAvaMediaPlayableTransitionEntryRole::Exit);
	PlayableTransition = TransitionBuilder.MakeTransition(this);

	// Mark exit only transitions to ensure they properly create the null behavior instances. 
	if (PlayableTransition && EnterPlayersWeak.IsEmpty())
	{
		PlayableTransition->SetTransitionFlags(EAvalanchePlayableTransitionFlags::ExitOnly);
	}
}

void UAvalanchePageTransition::LogDetailedTransitionInfo() const
{
	if (!UE_LOG_ACTIVE(LogAvaPlaylist, Verbose))
	{
		return;
	}
	
	using namespace UE::AvaMediaPlayback::Utils;
	UE_LOG(LogAvaPlaylist, Verbose, TEXT("%s Starting Page Transition \"%s\":"), *GetBriefFrameInfo(), *GetFullName());
	
	auto LogPlayers = [this](const TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& InPlayersWeak, const TCHAR* InCategory)
	{
		using namespace UE::AvaMedia::Rundown::PageTransition::Private;
		for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : InPlayersWeak)
		{
			if (const UAvalanchePagePlayer* Player = PlayerWeak.Get())
			{
				UE_LOG(LogAvaPlaylist, Verbose, TEXT("- %s Page: %d, %s."), InCategory, Player->PageId, *GetPrettyPageInfo(Player));
			}
		}
	};

	LogPlayers(EnterPlayersWeak, TEXT("Enter"));
	LogPlayers(PlayingPlayersWeak, TEXT("Playing"));
	LogPlayers(ExitPlayersWeak, TEXT("Exit"));
}

FString UAvalanchePageTransition::GetBriefTransitionDescription() const
{
	auto MakePageIdList = [](const TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& InPlayersWeak) -> FString
	{
		FString PageIdList;
		for (const TWeakObjectPtr<UAvalanchePagePlayer>& PlayerWeak : InPlayersWeak)
		{
			if (const UAvalanchePagePlayer* Player = PlayerWeak.Get())
			{
				PageIdList += FString::Printf(TEXT("%s%d"), PageIdList.IsEmpty() ? TEXT("") : TEXT(", "), Player->PageId);
			}
		}
		return PageIdList.IsEmpty() ? TEXT("None") : PageIdList;
	};

	const FString EnterPageList = MakePageIdList(EnterPlayersWeak);
	const FString PlayingPageList = MakePageIdList(PlayingPlayersWeak);
	const FString ExitPageList = MakePageIdList(ExitPlayersWeak);
	return FString::Printf(TEXT("Page Transition \"%s\": Enter Page(s): [%s], Playing Page(s): [%s], Exit Page(s): [%s]."),
		*GetFullName(), *EnterPageList, *PlayingPageList, *ExitPageList);
}

void UAvalanchePageTransition::RegisterToPlayableTransitionEvent()
{
	UAvalanchePlayable::OnTransitionEvent().RemoveAll(this);
	UAvalanchePlayable::OnTransitionEvent().AddUObject(this, &UAvalanchePageTransition::OnTransitionEvent);
}

void UAvalanchePageTransition::UnregisterFromPlayableTransitionEvent() const
{
	UAvalanchePlayable::OnTransitionEvent().RemoveAll(this);
}

void UAvalanchePageTransition::RegisterEnterPagePlayerEvents(UAvalanchePagePlayer* InPagePlayer)
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;

	if (!InPagePlayer)
	{
		return;
	}
	
	for (const UAvaRundownPlaybackInstancePlayer* InstancePlayer : InPagePlayer->InstancePlayers)
	{
		CachedTransitionLayers.Add(InstancePlayer->TransitionLayer.TagId);

		// Register this transition as a visibility constraint for the playable group.
		if (const UAvalanchePlayable* Playable = GetPlayable(InstancePlayer))
		{
			if (UAvaMediaPlayableGroup* PlayableGroup = Playable->GetPlayableGroup())
			{
				PlayableGroup->RegisterVisibilityConstraint(this);
			}
		}
		else if (UAvalanchePlayback* Playback = GetPlayback(InstancePlayer))
		{
			// If the playable is not created yet, register to the creation event.
			Playback->OnPlayableCreated.AddUObject(this, &UAvalanchePageTransition::OnPlayableCreated);
		}
	}
}

void UAvalanchePageTransition::UnregisterEnterPagePlayerEvents(UAvalanchePagePlayer* InPagePlayer)
{
	using namespace UE::AvaMedia::Rundown::PageTransition::Private;
	
	if (!InPagePlayer)
	{
		return;
	}
	
	InPagePlayer->ForEachInstancePlayer([this](const UAvaRundownPlaybackInstancePlayer* InInstancePlayer)
	{
		if (UAvalanchePlayback* Playback = GetPlayback(InInstancePlayer))
		{
			Playback->OnPlayableCreated.RemoveAll(this);
			Playback->ForEachPlayable([this](const UAvalanchePlayable* InPlayable)
			{
				if (UAvaMediaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
				{
					PlayableGroup->UnregisterVisibilityConstraint(this);
				}
			});
		}
	});
}

bool UAvalanchePageTransition::AddPagePlayer(UAvalanchePagePlayer* InPagePlayer, TArray<TWeakObjectPtr<UAvalanchePagePlayer>>& OutPagePlayersWeak)
{
	if (!InPagePlayer)
	{
		return false;	
	}

	OutPagePlayersWeak.Add(InPagePlayer);

	UpdateChannelName(InPagePlayer);

	return true;
}

void UAvalanchePageTransition::UpdateChannelName(const UAvalanchePagePlayer* InPagePlayer)
{
	check(InPagePlayer);
	
	if (ChannelName.IsNone())
	{
		ChannelName = InPagePlayer->ChannelFName;
	}
	else
	{
		using namespace UE::AvaMedia::Rundown::PageTransition::Private;

		// Validate the channel is the same.
		if (ChannelName != InPagePlayer->ChannelFName)
		{
			UE_LOG(LogAvaPlaylist, Error, TEXT("Page Transition \"%s\": Adding Page: %d, {%s} in a different channel than previous pages (\"%s\")."),
				*GetFullName(), InPagePlayer->PageId, *GetPrettyPageInfo(InPagePlayer), *ChannelName.ToString());
		}
	}
}

