// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvaRemotePlayableTransition.h"

#include "IAvaMediaModule.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/AvaMediaPlaybackClientDelegates.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playback/IAvaMediaPlaybackClient.h"
#include "Playback/Transition/AvaPlayableTransitionPrivate.h"

UAvaRemotePlayableTransition::~UAvaRemotePlayableTransition()
{
	UnregisterFromPlaybackClientDelegates();
}

bool UAvaRemotePlayableTransition::Start()
{
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	
	if (!AvaMediaModule.IsMediaPlaybackClientStarted())
	{
		TransitionId.Invalidate();
		return false;
	}

	if (!Super::Start())
	{
		return false;
	}

	TransitionId = FGuid::NewGuid();
	
	using namespace UE::AvaPlayableTransition::Private;
	TArray<FGuid> EnterInstanceIds = GetInstanceIds(EnterPlayablesWeak);
	TArray<FGuid> PlayingInstanceIds = GetInstanceIds(PlayingPlayablesWeak);
	TArray<FGuid> ExitInstanceIds = GetInstanceIds(ExitPlayablesWeak);
	TArray<FAvalancheRemoteControlValues> EnterValues;
	EnterValues.Reserve(EnterPlayableValues.Num());
	for (const TSharedPtr<FAvalancheRemoteControlValues>& Values : EnterPlayableValues)
	{
		EnterValues.Add(Values.IsValid() ? *Values : FAvalancheRemoteControlValues::GetDefaultEmpty());
	}
	
	IAvaMediaPlaybackClient& PlaybackClient = AvaMediaModule.GetMediaPlaybackClient();
	PlaybackClient.RequestPlayableTransitionStart(TransitionId, MoveTemp(EnterInstanceIds), MoveTemp(PlayingInstanceIds), MoveTemp(ExitInstanceIds), MoveTemp(EnterValues), ChannelName, TransitionFlags);
	RegisterToPlaybackClientDelegates();	// to get the transition events from the server side.

	using namespace UE::AvaMediaPlayback::Utils;
	UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Remote Playable Transition \"%s\" Id:\"%s\" starting."), *GetBriefFrameInfo(), *GetFullName(), *TransitionId.ToString());
	return true;
}

void UAvaRemotePlayableTransition::Stop()
{
	if (TransitionId.IsValid())
	{
		IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	
		if (AvaMediaModule.IsMediaPlaybackClientStarted())
		{
			AvaMediaModule.GetMediaPlaybackClient().RequestPlayableTransitionStop(TransitionId, ChannelName);
		}
	}

	Super::Stop();
}

void UAvaRemotePlayableTransition::RegisterToPlaybackClientDelegates()
{
	UE::AvaMediaPlaybackClient::Delegates::GetOnPlaybackTransitionEvent().RemoveAll(this);
	UE::AvaMediaPlaybackClient::Delegates::GetOnPlaybackTransitionEvent().AddUObject(this, &UAvaRemotePlayableTransition::HandlePlaybackTransitionEvent);
}

void UAvaRemotePlayableTransition::UnregisterFromPlaybackClientDelegates() const
{
	UE::AvaMediaPlaybackClient::Delegates::GetOnPlaybackTransitionEvent().RemoveAll(this);
}

bool UAvaRemotePlayableTransition::IsRunning() const
{
	return TransitionId.IsValid();	
}

void UAvaRemotePlayableTransition::HandlePlaybackTransitionEvent(IAvaMediaPlaybackClient& InPlaybackClient,
	const UE::AvaMediaPlaybackClient::Delegates::FPlaybackTransitionEventArgs& InArgs)
{
	if (InArgs.TransitionId != TransitionId)
	{
		return;
	}

	if (InArgs.InstanceId.IsValid())
	{
		if (UAvalanchePlayable* Playable = FindPlayable(InArgs.InstanceId))
		{
			// Relay locally through playable event.
			UAvalanchePlayable::OnTransitionEvent().Broadcast(Playable, this, InArgs.EventFlags);
		}
		else
		{
			UE_LOG(LogAvalanchePlayable, Error,
				TEXT("Remote Playable Transition \"%s\" doesn't have playable instance Id \"%s\"."),
				*TransitionId.ToString(), *InArgs.InstanceId.ToString());
		}
	}
	else
	{
		UAvalanchePlayable::OnTransitionEvent().Broadcast(nullptr, this, InArgs.EventFlags);
	}

	if (EnumHasAnyFlags(InArgs.EventFlags, EAvalanchePlayableTransitionEventFlags::Finished))
	{
		using namespace UE::AvaMediaPlayback::Utils;
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Remote Playable Transition \"%s\" Id:\"%s\" ended."), *GetBriefFrameInfo(), *GetFullName(), *TransitionId.ToString());
	}
}

