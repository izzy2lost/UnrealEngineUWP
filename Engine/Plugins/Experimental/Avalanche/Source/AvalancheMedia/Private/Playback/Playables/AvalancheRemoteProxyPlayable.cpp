// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Playables/AvalancheRemoteProxyPlayable.h"

#include "AvalancheBroadcast.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "IAvaMediaModule.h"
#include "Playback/AvaMediaPlayableGroupManager.h"
#include "Playback/AvaMediaPlaybackClientDelegates.h"
#include "Playback/IAvaMediaPlaybackClient.h"

#define LOCTEXT_NAMESPACE "AvalancheRemoteProxyPlayable"

bool UAvalancheRemoteProxyPlayable::LoadAsset(const FAvaSoftAssetPtr& InAvalancheSourceAsset, bool bInInitiallyVisible)
{
	if (!PlayableGroup)
	{
		return false;
	}
	
	SourceAssetPath = InAvalancheSourceAsset.ToSoftObjectPath();

	IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		const FString ChannelName = PlayingChannelName;
		const FSoftObjectPath& AssetPath = InAvalancheSourceAsset.ToSoftObjectPath();
		const TOptional<EAvaMediaPlaybackStatus> RemoteStatusOpt = PlaybackClient.GetRemotePlaybackStatus(InstanceId, AssetPath, ChannelName);
		const EAvaMediaPlaybackStatus RemoteStatus = RemoteStatusOpt.IsSet() ? RemoteStatusOpt.GetValue() : EAvaMediaPlaybackStatus::Unknown;

		const bool bCanLoad = RemoteStatus == EAvaMediaPlaybackStatus::Available
			|| RemoteStatus == EAvaMediaPlaybackStatus::Unknown;
		const bool bIsLoaded = RemoteStatus == EAvaMediaPlaybackStatus::Loading
			|| RemoteStatus == EAvaMediaPlaybackStatus::Loaded
			|| RemoteStatus == EAvaMediaPlaybackStatus::Starting
			|| RemoteStatus == EAvaMediaPlaybackStatus::Started;

		if (bCanLoad && !bIsLoaded)
		{
			PlaybackClient.RequestPlayback(InstanceId, AssetPath, ChannelName, EAvaMediaPlaybackAction::Load);
			PlaybackClient.RequestPlayback(InstanceId, AssetPath, ChannelName, EAvaMediaPlaybackAction::SetUserData, UserData);
		}
	}
	return true;
}

bool UAvalancheRemoteProxyPlayable::UnloadAsset()
{
	IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		PlaybackClient.RequestPlayback(InstanceId, SourceAssetPath, PlayingChannelName, EAvaMediaPlaybackAction::Unload);
	}
	return true;
}

namespace UE::AvalancheRemoteProxyPlayable::Private
{
	EAvalanchePlayableStatus GetPlayableStatus(EAvaMediaPlaybackStatus InPlaybackStatus)
	{
		switch (InPlaybackStatus)
		{
		case EAvaMediaPlaybackStatus::Unknown:
			return EAvalanchePlayableStatus::Unknown;
		case EAvaMediaPlaybackStatus::Missing:
		case EAvaMediaPlaybackStatus::Syncing:
		case EAvaMediaPlaybackStatus::Available:
			return EAvalanchePlayableStatus::Unloaded;
		case EAvaMediaPlaybackStatus::Loading:
			return EAvalanchePlayableStatus::Loading;
		case EAvaMediaPlaybackStatus::Loaded:
			return EAvalanchePlayableStatus::Loaded;
		case EAvaMediaPlaybackStatus::Starting:
			return EAvalanchePlayableStatus::Loaded;
		case EAvaMediaPlaybackStatus::Started:
			return EAvalanchePlayableStatus::Visible;
		case EAvaMediaPlaybackStatus::Stopping:
			return EAvalanchePlayableStatus::Loaded;
		case EAvaMediaPlaybackStatus::Unloading:
			return EAvalanchePlayableStatus::Unloaded;
		case EAvaMediaPlaybackStatus::Error:
		default:
			return EAvalanchePlayableStatus::Error;
		}
	}
}

EAvalanchePlayableStatus UAvalancheRemoteProxyPlayable::GetPlayableStatus() const
{
	IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	const TArray<FString> OnlineServers = GetOnlineServerForChannel(PlayingChannelFName);
	
	for (const FString& Server : OnlineServers)
	{
		TOptional<EAvaMediaPlaybackStatus> PlaybackStatus = PlaybackClient.GetRemotePlaybackStatus(InstanceId, SourceAssetPath, PlayingChannelName, Server);

		if (!PlaybackStatus.IsSet())
		{
			PlaybackClient.RequestPlayback(InstanceId, SourceAssetPath, PlayingChannelName, EAvaMediaPlaybackAction::Status);
			PlaybackStatus = EAvaMediaPlaybackStatus::Unknown;
		}

		// TODO: reconcile forked channels.
		return UE::AvalancheRemoteProxyPlayable::Private::GetPlayableStatus(PlaybackStatus.GetValue());
	}
	
	return EAvalanchePlayableStatus::Unknown;
}

IAvaSceneInterface* UAvalancheRemoteProxyPlayable::GetSceneInterface() const
{
	return nullptr;
}

EAvalanchePlayableCommandResult UAvalancheRemoteProxyPlayable::ExecuteAnimationCommand(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings)
{
	IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	
	// If an animation event was locally scheduled on a remote playable,
	// we need to propagate the event.
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		switch (InAnimAction)
		{
		case EAvaMediaAnimAction::None:
			break;
		case EAvaMediaAnimAction::Play:
		case EAvaMediaAnimAction::PreviewFrame:
			PlaybackClient.RequestAnimPlayback(InstanceId, SourceAssetPath, PlayingChannelName, InAnimPlaySettings);
			break;

		case EAvaMediaAnimAction::Continue:
		case EAvaMediaAnimAction::Stop:
		case EAvaMediaAnimAction::CameraCut:
			PlaybackClient.RequestAnimAction(InstanceId, SourceAssetPath, PlayingChannelName, InAnimPlaySettings.AnimationName.ToString(), InAnimAction);
			break;

		default:
			UE_LOG(LogAvalanchePlayable, Warning,
				TEXT("Animation command action \"%s\" for asset \"%s\" on channel \"%s\" is not implemented."),
				*StaticEnum<EAvaMediaAnimAction>()->GetValueAsString(InAnimAction),
				*SourceAssetPath.ToString(), *PlayingChannelName);
			break;
		}
	}
	return EAvalanchePlayableCommandResult::Executed;
}

EAvalanchePlayableCommandResult UAvalancheRemoteProxyPlayable::UpdateRemoteControlCommand(const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues)
{
	IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		PlaybackClient.RequestRemoteControlUpdate(InstanceId, SourceAssetPath, PlayingChannelName, *InRemoteControlValues);
	}
	return EAvalanchePlayableCommandResult::Executed;
}

bool UAvalancheRemoteProxyPlayable::ApplyCamera()
{
	return false;
}

void UAvalancheRemoteProxyPlayable::SetUserData(const FString& InUserData)
{
	if (UserData != InUserData)
	{
		IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
		if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
		{
			PlaybackClient.RequestPlayback(InstanceId, GetSourceAssetPath(), PlayingChannelName, EAvaMediaPlaybackAction::SetUserData, InUserData);
		}
	}
	
	Super::SetUserData(InUserData);
}

bool UAvalancheRemoteProxyPlayable::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	// We keep track of the channel this playable is part of.
	PlayingChannelFName = InPlayableInfo.ChannelName;
	PlayingChannelName = InPlayableInfo.ChannelName.ToString();
	
	constexpr bool bIsRemoteProxy = true;

	// Remote playables have proxy playable groups imitating the same setup as local ones.
	switch (InPlayableInfo.SourceAsset.GetAssetType())
	{
	case EAvalancheAssetType::Blueprint:
		{
			UAvaMediaPlayableGroup::FPlayableGroupCreationInfo PlayableGroupCreationInfo;
			PlayableGroupCreationInfo.PlayableGroupManager = InPlayableInfo.PlayableGroupManager;
			PlayableGroupCreationInfo.SourceAssetPath = InPlayableInfo.SourceAsset.ToSoftObjectPath();
			PlayableGroupCreationInfo.ChannelName = InPlayableInfo.ChannelName;
			PlayableGroupCreationInfo.bIsRemoteProxy = bIsRemoteProxy;
			PlayableGroupCreationInfo.bIsSharedGroup = false;
			
			PlayableGroup = UAvaMediaPlayableGroup::MakePlayableGroup(InPlayableInfo.PlayableGroupManager, PlayableGroupCreationInfo);
		}
		break;
	case EAvalancheAssetType::World:
		PlayableGroup = InPlayableInfo.PlayableGroupManager->GetOrCreateSharedLevelGroup(InPlayableInfo.ChannelName, bIsRemoteProxy);
		break;
	default:
		UE_LOG(LogAvalanchePlayable, Error, TEXT("Asset \"%s\" is an unsupported type."), *InPlayableInfo.SourceAsset.ToSoftObjectPath().ToString());
		break;
	}
	
	const bool bInitSucceeded = Super::InitPlayable(InPlayableInfo);
	if (bInitSucceeded)
	{
		RegisterClientEventHandlers();
	}
	return bInitSucceeded;
}

void UAvalancheRemoteProxyPlayable::OnPlay()
{
	IAvaMediaModule& AvaMediaModule = IAvaMediaModule::Get();
	if (!AvaMediaModule.IsMediaPlaybackClientStarted())
	{
		return;
	}
	
	IAvaMediaPlaybackClient& Client = AvaMediaModule.GetMediaPlaybackClient();
	
	if (Client.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		const FString ChannelName = PlayingChannelName;
		const TOptional<EAvaMediaPlaybackStatus> RemoteStatusOpt = Client.GetRemotePlaybackStatus(InstanceId, SourceAssetPath, ChannelName);
		const EAvaMediaPlaybackStatus RemoteStatus = RemoteStatusOpt.IsSet() ? RemoteStatusOpt.GetValue() : EAvaMediaPlaybackStatus::Unknown;
		// TODO: rework this logic.
		if (RemoteStatus == EAvaMediaPlaybackStatus::Available
			|| RemoteStatus == EAvaMediaPlaybackStatus::Loading || RemoteStatus == EAvaMediaPlaybackStatus::Loaded
			|| RemoteStatus == EAvaMediaPlaybackStatus::Unknown || RemoteStatus == EAvaMediaPlaybackStatus::Stopping
			|| RemoteStatus == EAvaMediaPlaybackStatus::Unloading)
		{
			Client.RequestPlayback(InstanceId, SourceAssetPath, ChannelName, EAvaMediaPlaybackAction::Start);
		}
	}
}

void UAvalancheRemoteProxyPlayable::OnEndPlay()
{
	IAvaMediaPlaybackClient& PlaybackClient = IAvaMediaModule::Get().GetMediaPlaybackClient();
	if (PlaybackClient.HasAnyServerOnlineForChannel(PlayingChannelFName))
	{
		PlaybackClient.RequestPlayback(InstanceId, SourceAssetPath, PlayingChannelName, EAvaMediaPlaybackAction::Stop);
	}
}

void UAvalancheRemoteProxyPlayable::BeginDestroy()
{
	UnregisterClientEventHandlers();
	Super::BeginDestroy();
}

void UAvalancheRemoteProxyPlayable::RegisterClientEventHandlers()
{
	using namespace UE::AvaMediaPlaybackClient::Delegates;
	GetOnPlaybackSequenceEvent().RemoveAll(this);
	GetOnPlaybackSequenceEvent().AddUObject(this, &UAvalancheRemoteProxyPlayable::HandleAvaMediaPlaybackSequenceEvent);
}

void UAvalancheRemoteProxyPlayable::UnregisterClientEventHandlers() const
{
	using namespace UE::AvaMediaPlaybackClient::Delegates;
	GetOnPlaybackSequenceEvent().RemoveAll(this);
}

TArray<FString> UAvalancheRemoteProxyPlayable::GetOnlineServerForChannel(const FName& InChannelName)
{
	const FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(InChannelName);
	const TArray<UMediaOutput*>& Outputs = Channel.GetMediaOutputs();
	TArray<FString> OnlineServers;
	OnlineServers.Reserve(Outputs.Num());
	
	for (const UMediaOutput* Output : Outputs)
	{
		if (Channel.IsMediaOutputRemote(Output) && Channel.GetMediaOutputState(Output) != EAvaMediaOutputState::Offline)
		{
			OnlineServers.AddUnique(Channel.GetMediaOutputServerName(Output));
		}
	}
	return OnlineServers;
}

void UAvalancheRemoteProxyPlayable::HandleAvaMediaPlaybackSequenceEvent(IAvaMediaPlaybackClient& InPlaybackClient,
	const UE::AvaMediaPlaybackClient::Delegates::FPlaybackSequenceEventArgs& InEventArgs)
{
	if (InEventArgs.InstanceId == InstanceId && InEventArgs.ChannelName == PlayingChannelName)
	{
		const FName SequenceName(InEventArgs.SequenceName);
		OnSequenceEventDelegate.Broadcast(this, SequenceName, InEventArgs.EventType);
	}
}

#undef LOCTEXT_NAMESPACE
