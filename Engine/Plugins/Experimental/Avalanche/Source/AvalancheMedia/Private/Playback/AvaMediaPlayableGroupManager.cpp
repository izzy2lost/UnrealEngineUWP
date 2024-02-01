// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvaMediaPlayableGroupManager.h"

#include "AvalancheBroadcast.h"
#include "Framework/AvaGameInstance.h"
#include "Misc/CoreDelegates.h"
#include "Misc/TimeGuard.h"
#include "OutputDevices/AvaRenderTargetMediaUtils.h"
#include "Playback/AvaMediaPlayableGroup.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AvaMediaPlayableGroupManager"

UAvaMediaPlayableGroup* UAvaMediaChannelPlayableGroupManager::GetOrCreateSharedLevelGroup(bool bInIsRemoteProxy)
{
	if (UAvaMediaPlayableGroup* ExistingPlayableGroup = bInIsRemoteProxy ? SharedRemoteProxyLevelGroupWeak.Get() : SharedLevelGroupWeak.Get())
	{
		return ExistingPlayableGroup;
	}
	
	UAvaMediaPlayableGroup::FPlayableGroupCreationInfo PlayableGroupCreationInfo;
	PlayableGroupCreationInfo.PlayableGroupManager = GetPlayableGroupManager();
	PlayableGroupCreationInfo.ChannelName = ChannelName;
	PlayableGroupCreationInfo.bIsRemoteProxy = bInIsRemoteProxy;
	PlayableGroupCreationInfo.bIsSharedGroup = true;

	UAvaMediaPlayableGroup* NewPlayableGroup = UAvaMediaPlayableGroup::MakePlayableGroup(GetPlayableGroupManager(), PlayableGroupCreationInfo);

	// Keep track of the shared playable group.
	if (bInIsRemoteProxy)
	{
		SharedRemoteProxyLevelGroupWeak = NewPlayableGroup;
	}
	else
	{
		SharedLevelGroupWeak = NewPlayableGroup;
	}
	
	return NewPlayableGroup;
}

UAvaMediaPlayableGroupManager* UAvaMediaChannelPlayableGroupManager::GetPlayableGroupManager() const
{
	return Cast<UAvaMediaPlayableGroupManager>(GetOuter());	
}

void UAvaMediaChannelPlayableGroupManager::Shutdown()
{
	SharedRemoteProxyLevelGroupWeak.Reset();
	SharedLevelGroupWeak.Reset();
}

void UAvaMediaChannelPlayableGroupManager::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UAvaMediaPlayableGroupManager::Init()
{
	if (!UAvaGameInstance::GetOnEndPlay().IsBoundToObject(this))
	{
		UAvaGameInstance::GetOnEndPlay().AddUObject(this, &UAvaMediaPlayableGroupManager::OnGameInstanceEndPlay);
	}
}

void UAvaMediaPlayableGroupManager::Shutdown()
{
	for (TPair<FName, TObjectPtr<UAvaMediaChannelPlayableGroupManager>>& Pair : ChannelManagers)
	{
		if (UAvaMediaChannelPlayableGroupManager* const ChannelManager = Pair.Value)
		{
			ChannelManager->Shutdown();
		}
	}
	UAvaGameInstance::GetOnEndPlay().RemoveAll(this);
}

void UAvaMediaPlayableGroupManager::Tick(double InDeltaSeconds)
{
	SCOPE_TIME_GUARD(TEXT("UAvaMediaPlayableGroupManager::Tick"));
	UpdateLevelStreaming();
	TickTransitions(InDeltaSeconds);
}

UAvaMediaChannelPlayableGroupManager* UAvaMediaPlayableGroupManager::FindOrAddChannelManager(const FName& InChannelName)
{
	UAvaMediaChannelPlayableGroupManager* ChannelManager = FindChannelManager(InChannelName);
	if (!ChannelManager)
	{
		ChannelManager = NewObject<UAvaMediaChannelPlayableGroupManager>(this);
		ChannelManager->ChannelName = InChannelName;
		ChannelManagers.Add(InChannelName, ChannelManager);
	}
	return ChannelManager;
}

void UAvaMediaPlayableGroupManager::RegisterForLevelStreamingUpdate(UAvaMediaPlayableGroup* InPlayableGroup)
{
	if (ensure(!bIsUpdatingStreaming))
	{
		GroupsToUpdateStreaming.Add(InPlayableGroup);
	}
}

void UAvaMediaPlayableGroupManager::UnregisterFromLevelStreamingUpdate(UAvaMediaPlayableGroup* InPlayableGroup)
{
	if (!bIsUpdatingStreaming)
	{
		GroupsToUpdateStreaming.Remove(InPlayableGroup);
	}
}

void UAvaMediaPlayableGroupManager::RegisterForTransitionTicking(UAvaMediaPlayableGroup* InPlayableGroup)
{
	if (ensure(!bIsTickingTransitions))
	{
		GroupsToTickTransitions.Add(InPlayableGroup);
	}
}

void UAvaMediaPlayableGroupManager::UnregisterFromTransitionTicking(UAvaMediaPlayableGroup* InPlayableGroup)
{
	if (!bIsTickingTransitions)
	{
		GroupsToTickTransitions.Remove(InPlayableGroup);
	}
}

void UAvaMediaPlayableGroupManager::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

void UAvaMediaPlayableGroupManager::OnGameInstanceEndPlay(UAvaGameInstance* InGameInstance, FName InChannelName)
{
	FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannelMutable(InChannelName);
	if (!Channel.IsValidChannel())
	{
		return;
	}

	// In the current design, there is only one playable group active on a channel, i.e.
	// the graph of playables and groups must end on a root "playable group".
	// This is resolved by the playback graph(s) running on that channel.
	//
	// When we receive an EndPlay event from a game instance for a given channel, we need to
	// check if that is the current root playable group, and if it is, clear out the
	// channels association with it.
	//
	// If the channel is live, it will then start rendering the placeholder graphic, if
	// configured to do so, on the next slate tick.
	
	if (const UAvaMediaPlayableGroup* PlayableGroup = Channel.GetLastActivePlayableGroup())
	{
		if (PlayableGroup->GetGameInstance() == InGameInstance)
		{
			Channel.UpdateRenderTarget(nullptr, nullptr);
			Channel.UpdateAudioDevice(FAudioDeviceHandle());
		}
	}

	// If the channel is not live, the placeholder graphic doesn't render
	// so we need to explicitly clear the channel here.
	if (Channel.GetState() != EAvaChannelState::Live)
	{
		if (UTextureRenderTarget2D* const RenderTarget = Channel.GetCurrentRenderTarget(true))
		{
			UE::AvaRenderTargetMediaUtils::ClearRenderTarget(RenderTarget);
		}
	}
}

void UAvaMediaPlayableGroupManager::UpdateLevelStreaming()
{
	TGuardValue TransitionsTickGuard(bIsUpdatingStreaming, true);		
	for (TSet<TWeakObjectPtr<UAvaMediaPlayableGroup>>::TIterator GroupIterator(GroupsToUpdateStreaming); GroupIterator; ++GroupIterator)
	{
		const UAvaMediaPlayableGroup* GroupToUpdate = GroupIterator->Get();
		// We only update streaming if the group is not playing.
		if (GroupToUpdate && !GroupToUpdate->IsWorldPlaying())
		{
			// World may not be created yet.
			if (UWorld* PlayWorld = GroupToUpdate->GetPlayWorld())
			{
				// (This is normally updated by the game viewport client when the world is playing.)
				PlayWorld->UpdateLevelStreaming();

				// Check if still has streaming. If not, from the list.
				if (!PlayWorld->HasStreamingLevelsToConsider())
				{
					GroupIterator.RemoveCurrent();
				}
			}
		}
		else
		{
			GroupIterator.RemoveCurrent();
		}
	}
}

void UAvaMediaPlayableGroupManager::TickTransitions(double InDeltaSeconds)
{
	TGuardValue TransitionsTickGuard(bIsTickingTransitions, true);
	for (TSet<TWeakObjectPtr<UAvaMediaPlayableGroup>>::TIterator GroupIterator(GroupsToTickTransitions); GroupIterator; ++GroupIterator)
	{
		bool bHasTransitions = false;
		if (UAvaMediaPlayableGroup* GroupToTick = GroupIterator->Get())
		{
			GroupToTick->TickTransitions(InDeltaSeconds);
			bHasTransitions = GroupToTick->HasTransitions();
		}

		// Group automatically deregister if stale or they don't have active transitions.
		if (!bHasTransitions)
		{
			GroupIterator.RemoveCurrent();
		}
	}
}

#undef LOCTEXT_NAMESPACE