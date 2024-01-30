// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvaMediaServerPlaybackTransition.h"

#include "IAvaMediaModule.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvaMediaPlaybackServer.h"
#include "Playback/Transition/AvalanchePlayableTransition.h"

namespace UE::AvaMediaServerPlaybackTransition::Private
{
	FString GetPrettyPlaybackInstanceInfo(const FAvaMediaPlaybackInstance* InPlaybackInstance)
	{
		if (InPlaybackInstance)
		{
			return FString::Printf(TEXT("Id:%s, Asset:%s, Channel:%s, UserData:\"%s\""),
				*InPlaybackInstance->GetInstanceId().ToString(),
				*InPlaybackInstance->GetSourcePath().ToString(),
				*InPlaybackInstance->GetChannelName(),
				*InPlaybackInstance->GetInstanceUserData());
		}
		return TEXT("");
	}

	UAvalanchePlayable* GetPlayable(const FAvaMediaPlaybackInstance* InPlaybackInstance)
	{
		if (InPlaybackInstance && InPlaybackInstance->GetPlayback())
		{
			return InPlaybackInstance->GetPlayback()->GetFirstPlayable();
		}
		return nullptr;
	}

	TSharedPtr<FAvaMediaPlaybackInstance> FindInstanceForPlayable(const TArray<TWeakPtr<FAvaMediaPlaybackInstance>>& InPlaybackInstancesWeak, const UAvalanchePlayable* InPlayable)
	{
		for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin();
			if (GetPlayable(Instance.Get()) == InPlayable)
			{
				return Instance;
			}
		}
		return nullptr;
	}
}

void UAvaMediaServerPlaybackTransition::SetEnterValues(const TArray<FAvalancheRemoteControlValues>& InEnterValues)
{
	EnterValues.Reserve(InEnterValues.Num());
	for (const FAvalancheRemoteControlValues& Values : InEnterValues)
	{
		EnterValues.Add(MakeShared<FAvalancheRemoteControlValues>(Values));
	}
}

bool UAvaMediaServerPlaybackTransition::AddEnterInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance)
{
	if (!InPlaybackInstance)
	{
		return false;
	}
	
	// Register this transition as a visibility constraint.
	using namespace UE::AvaMediaServerPlaybackTransition::Private;
	if (const UAvalanchePlayable* Playable = GetPlayable(InPlaybackInstance.Get()))
	{
		if (UAvaMediaPlayableGroup* PlayableGroup = Playable->GetPlayableGroup())
		{
			PlayableGroup->RegisterVisibilityConstraint(this);
		}
	}
	else if (UAvalanchePlayback* Playback = InPlaybackInstance->GetPlayback())
	{
		// If the playable is not created yet, register to the creation event.
		Playback->OnPlayableCreated.AddUObject(this, &UAvaMediaServerPlaybackTransition::OnPlayableCreated);
	}
	
	return AddPlaybackInstance(InPlaybackInstance, EnterPlaybackInstancesWeak);
}

bool UAvaMediaServerPlaybackTransition::AddPlayingInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance)
{
	return AddPlaybackInstance(InPlaybackInstance, PlayingPlaybackInstancesWeak);
}

bool UAvaMediaServerPlaybackTransition::AddExitInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance)
{
	return AddPlaybackInstance(InPlaybackInstance, ExitPlaybackInstancesWeak);
}

void UAvaMediaServerPlaybackTransition::TryResolveInstances(const FAvaMediaPlaybackServer& InPlaybackServer)
{
	if (EnterPlaybackInstancesWeak.Num() < EnterInstanceIds.Num())
	{
		for (const FGuid& InstanceId : EnterInstanceIds)
		{
			if (TSharedPtr<FAvaMediaPlaybackInstance> Instance = InPlaybackServer.FindActivePlaybackInstance(InstanceId))
			{
				if (!EnterPlaybackInstancesWeak.Contains(Instance.ToWeakPtr()))
				{
					AddEnterInstance(Instance);
				}
			}
		}
	}	
}

bool UAvaMediaServerPlaybackTransition::IsVisibilityConstrained(const UAvalanchePlayable* InPlayable) const
{
	using namespace UE::AvaMediaServerPlaybackTransition::Private;
	bool bAllPlayablesLoaded = true;
	bool bIsPlayableInThisTransition = false;
	
	for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : EnterPlaybackInstancesWeak)
	{
		if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin())
		{
			if (const UAvalanchePlayable* Playable = GetPlayable(Instance.Get()))
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
		}
	}
	return bIsPlayableInThisTransition && !bAllPlayablesLoaded;
}


bool UAvaMediaServerPlaybackTransition::CanStart(bool& bOutShouldDiscard) const
{
	using namespace UE::AvaMediaServerPlaybackTransition::Private;

	if (EnterPlaybackInstancesWeak.Num() < EnterInstanceIds.Num())
	{
		bOutShouldDiscard = false;
		return false;
	}

	for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : EnterPlaybackInstancesWeak)
	{
		if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin())
		{
			if (const UAvalanchePlayable* Playable = GetPlayable(Instance.Get()))
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

				// Asset status must be visible to run the command.
				// If not visible, the components are not yet added to the world.
				if (PlayableStatus != EAvalanchePlayableStatus::Visible)
				{
					// Keep the command in the queue for next tick.
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

void UAvaMediaServerPlaybackTransition::Start()
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

void UAvaMediaServerPlaybackTransition::Stop()
{
	if (PlayableTransition)
	{
		PlayableTransition->Stop();
		PlayableTransition = nullptr;
	}

	for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : EnterPlaybackInstancesWeak)
	{
		if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin())
		{
			if (UAvalanchePlayback* Playback = Instance->GetPlayback())
			{
				Playback->OnPlayableCreated.RemoveAll(this);
			}
		}
	}
	
	UnregisterFromPlayableTransitionEvent();
	
	// Remove transition from server.
	if (const TSharedPtr<FAvaMediaPlaybackServer> PlaybackServer = IAvaMediaModule::Get().GetMediaPlaybackServer())
	{
		if (!PlaybackServer->RemovePlaybackInstanceTransition(TransitionId))
		{
			UE_LOG(LogAvaPlaybackServer, Error,
				TEXT("Playback Transition {%s} Error: Was not found in server's active transitions. "), *GetPrettyTransitionInfo());
		}
	}
}

bool UAvaMediaServerPlaybackTransition::IsRunning() const
{
	return PlayableTransition ? PlayableTransition->IsRunning() : false;
}

FString UAvaMediaServerPlaybackTransition::GetPrettyTransitionInfo() const
{
	return FString::Printf(TEXT("Id:%s, Channel:%s, Client:%s"),
		*TransitionId.ToString(), *ChannelName.ToString(), *ClientName);
}

FString UAvaMediaServerPlaybackTransition::GetBriefTransitionDescription() const
{
	auto MakeInstanceIdList = [](const TArray<TWeakPtr<FAvaMediaPlaybackInstance>>& InPlaybackInstancesWeak) -> FString
	{
		FString InstanceIdList;
		for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin())
			{
				InstanceIdList += FString::Printf(TEXT("%s%s"), InstanceIdList.IsEmpty() ? TEXT("") : TEXT(", "), *Instance->GetInstanceId().ToString());
			}
		}
		return InstanceIdList.IsEmpty() ? TEXT("None") : InstanceIdList;
	};
	
	const FString EnterInstanceList = MakeInstanceIdList(EnterPlaybackInstancesWeak);
	const FString PlayingInstanceList = MakeInstanceIdList(PlayingPlaybackInstancesWeak);
	const FString ExitInstanceList = MakeInstanceIdList(ExitPlaybackInstancesWeak);

	return FString::Printf(TEXT("Enter Instance(s): [%s], Playing Instance(s): [%s], Exit Instance(s): [%s]."), *EnterInstanceList, *PlayingInstanceList, *ExitInstanceList);
}

TSharedPtr<FAvaMediaPlaybackInstance> UAvaMediaServerPlaybackTransition::FindInstanceForPlayable(const UAvalanchePlayable* InPlayable)
{
	using namespace UE::AvaMediaServerPlaybackTransition;
	if (!InPlayable)
	{
		return nullptr;
	}

	TSharedPtr<FAvaMediaPlaybackInstance> Instance =  Private::FindInstanceForPlayable(EnterPlaybackInstancesWeak, InPlayable);
	if (Instance)
	{
		return Instance;
	}
	
	Instance =  Private::FindInstanceForPlayable(PlayingPlaybackInstancesWeak, InPlayable);
	if (Instance)
	{
		return Instance;
	}

	Instance =  Private::FindInstanceForPlayable(ExitPlaybackInstancesWeak, InPlayable);
	if (Instance)
	{
		return Instance;
	}
	return nullptr;
}

void UAvaMediaServerPlaybackTransition::OnTransitionEvent(UAvalanchePlayable* InPlayable, UAvalanchePlayableTransition* InTransition, EAvalanchePlayableTransitionEventFlags InTransitionFlags)
{
	using namespace UE::AvaMediaServerPlaybackTransition::Private;
	
	// not this transition.
	if (InTransition != PlayableTransition || PlayableTransition == nullptr)
	{
		return;
	}

	const TSharedPtr<FAvaMediaPlaybackServer> PlaybackServer = IAvaMediaModule::Get().GetMediaPlaybackServer();

	// Find the page player for this playable
	if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = FindInstanceForPlayable(InPlayable))
	{
		// Relay the transition event back to the client
		if (PlaybackServer)
		{
			PlaybackServer->SendPlayableTransitionEvent(TransitionId, InPlayable->GetInstanceId(), InTransitionFlags, ChannelName, ClientName);
		}
		
		if (EnumHasAnyFlags(InTransitionFlags, EAvalanchePlayableTransitionEventFlags::StopPlayable))
		{
			// Validating that we are not removing an "enter" playable.
			if (PlayableTransition->IsEnterPlayable(InPlayable))
			{
				UE_LOG(LogAvaPlaybackServer, Error,
					TEXT("Playback Transition {%s} Error: An \"enter\" playable is being discarded for instance {%s}."),
					*GetPrettyTransitionInfo(), *GetPrettyPlaybackInstanceInfo(Instance.Get()));
			}

			// See UAvalanchePagePlayer::Stop()
			const EAvaPlaybackStopOptions PlaybackStopOptions = bUnloadDiscardedInstances ?
				EAvaPlaybackStopOptions::Default | EAvaPlaybackStopOptions::Unload : EAvaPlaybackStopOptions::Default;	
			Instance->GetPlayback()->Stop(PlaybackStopOptions);
			
			if (bUnloadDiscardedInstances)
			{
				Instance->Unload();
				// Remove instance from the server.
				if (PlaybackServer)
				{
					if (!PlaybackServer->RemoveActivePlaybackInstance(Instance->GetInstanceId()))
					{
						UE_LOG(LogAvaPlaybackServer, Error,
							TEXT("Playback Transition {%s} Error: \"exit\" instance {%s} was not found in server's active instances. "),
							*GetPrettyTransitionInfo(), *GetPrettyPlaybackInstanceInfo(Instance.Get()));
					}
				}
			}
			else
			{
				Instance->Recycle();
			}
		}
	}

	if (EnumHasAnyFlags(InTransitionFlags, EAvalanchePlayableTransitionEventFlags::Finished))
	{
		if (PlaybackServer)
		{
			PlaybackServer->SendPlayableTransitionEvent(TransitionId, FGuid(), InTransitionFlags, ChannelName, ClientName);
		}
		
		Stop();
	}
}

void UAvaMediaServerPlaybackTransition::OnPlayableCreated(UAvalanchePlayback* InPlayback, UAvalanchePlayable* InPlayable)
{
	if (UAvaMediaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
	{
		PlayableGroup->RegisterVisibilityConstraint(this);
	}
}

void UAvaMediaServerPlaybackTransition::MakePlayableTransition()
{
	using namespace UE::AvaMediaServerPlaybackTransition::Private;

	FAvaPlayableTransitionBuilder TransitionBuilder;

	auto AddInstancesToBuilder = [&TransitionBuilder, this](const TArray<TWeakPtr<FAvaMediaPlaybackInstance>>& InPlaybackInstancesWeak, const TCHAR* InCategory, EAvaMediaPlayableTransitionEntryRole InEntryRole)
	{
		using namespace UE::AvaMediaServerPlaybackTransition::Private;
		int32 ArrayIndex = 0;
		for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin())
			{
				if (UAvalanchePlayable* Playable = GetPlayable(Instance.Get()))
				{
					const bool bPlayableAdded = TransitionBuilder.AddPlayable(Playable, InEntryRole);
					if (InEntryRole == EAvaMediaPlayableTransitionEntryRole::Enter && bPlayableAdded)
					{
						TransitionBuilder.AddEnterPlayableValues(EnterValues.IsValidIndex(ArrayIndex) ? EnterValues[ArrayIndex] : nullptr);	
					}
				}
				else
				{
					// If this happens, likely the playable is not yet loaded.
					UE_LOG(LogAvaPlaybackServer, Error,
						TEXT("Playback Transition {%s} Error: Failed to retrieve \"%s\" playable for instance {%s}."),
						*GetPrettyTransitionInfo(), InCategory, *GetPrettyPlaybackInstanceInfo(Instance.Get()));
				}
			}
			++ArrayIndex;
		}
	};
 
	AddInstancesToBuilder(EnterPlaybackInstancesWeak, TEXT("Enter"), EAvaMediaPlayableTransitionEntryRole::Enter);
	AddInstancesToBuilder(PlayingPlaybackInstancesWeak, TEXT("Playing"), EAvaMediaPlayableTransitionEntryRole::Playing);
	AddInstancesToBuilder(ExitPlaybackInstancesWeak, TEXT("Exit"), EAvaMediaPlayableTransitionEntryRole::Exit);
	PlayableTransition = TransitionBuilder.MakeTransition(this);

	if (PlayableTransition)
	{
		PlayableTransition->SetTransitionFlags(TransitionFlags);
	}
}

void UAvaMediaServerPlaybackTransition::LogDetailedTransitionInfo() const
{
	UE_LOG(LogAvaPlaybackServer, Verbose, TEXT("Playback Transition {%s}:"), *GetPrettyTransitionInfo());

	auto LogInstances = [](const TArray<TWeakPtr<FAvaMediaPlaybackInstance>>& InPlaybackInstancesWeak, const TCHAR* InCategory)
	{
		using namespace UE::AvaMediaServerPlaybackTransition::Private;

		for (const TWeakPtr<FAvaMediaPlaybackInstance>& InstanceWeak : InPlaybackInstancesWeak)
		{
			if (const TSharedPtr<FAvaMediaPlaybackInstance> Instance = InstanceWeak.Pin())
			{
				UE_LOG(LogAvaPlaybackServer, Verbose, TEXT("- %s Instance: {%s}."), InCategory, *GetPrettyPlaybackInstanceInfo(Instance.Get()));
			}
		}
	};

	LogInstances(EnterPlaybackInstancesWeak, TEXT("Enter"));
	LogInstances(PlayingPlaybackInstancesWeak, TEXT("Playing"));
	LogInstances(ExitPlaybackInstancesWeak, TEXT("Exit"));
}

void UAvaMediaServerPlaybackTransition::RegisterToPlayableTransitionEvent()
{
	UAvalanchePlayable::OnTransitionEvent().RemoveAll(this);
	UAvalanchePlayable::OnTransitionEvent().AddUObject(this, &UAvaMediaServerPlaybackTransition::OnTransitionEvent);
}

void UAvaMediaServerPlaybackTransition::UnregisterFromPlayableTransitionEvent() const
{
	UAvalanchePlayable::OnTransitionEvent().RemoveAll(this);
}

bool UAvaMediaServerPlaybackTransition::AddPlaybackInstance(const TSharedPtr<FAvaMediaPlaybackInstance>& InPlaybackInstance, TArray<TWeakPtr<FAvaMediaPlaybackInstance>>& OutPlaybackInstancesWeak)
{
	if (!InPlaybackInstance)
	{
		return false;	
	}

	OutPlaybackInstancesWeak.Add(InPlaybackInstance);
	UpdateChannelName(InPlaybackInstance.Get());
	return true;
}

void UAvaMediaServerPlaybackTransition::UpdateChannelName(const FAvaMediaPlaybackInstance* InPlaybackInstance)
{
	if (ChannelName.IsNone())
	{
		ChannelName = InPlaybackInstance->GetChannelFName();
	}
	else
	{
		using namespace UE::AvaMediaServerPlaybackTransition::Private;

		// Validate the channel is the same.
		if (ChannelName != InPlaybackInstance->GetChannelFName())
		{
			UE_LOG(LogAvaPlaybackServer, Error,
				TEXT("Playback Transition {%s}: Adding Playback Instance {%s} in a different channel than previous playback instance (\"%s\")."),
				*GetPrettyTransitionInfo(), *GetPrettyPlaybackInstanceInfo(InPlaybackInstance), *ChannelName.ToString());
		}
	}
}