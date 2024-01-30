// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaBroadcastProfile.h"
#include "AvalancheBroadcast.h"

#define LOCTEXT_NAMESPACE "AvalancheBroadcast"

FAvaBroadcastProfile FAvaBroadcastProfile::NullProfile(nullptr, NAME_None);

FAvaBroadcastProfile::FAvaBroadcastProfile(UAvalancheBroadcast* InBroadcast, FName InProfileName)
	: ParentBroadcastWeak(InBroadcast)
	, ProfileName(InProfileName)
{
}

void FAvaBroadcastProfile::BeginDestroy()
{
	for (FAvaOutputChannel& Channel : GetLocalChannels())
	{
		Channel.ReleasePlaceholderResources();
		Channel.ReleaseOutputs();
		Channel.ReleasePlaceholderRenderTargets();
	}
}

UAvalancheBroadcast& FAvaBroadcastProfile::GetBroadcast() const
{
	// Note: during the migration towards a non-singleton UAvalancheBroadcast,
	// a fallback path to the singleton is kept, but it has an ensure to try and catch
	// the code paths that lead to this.
	UAvalancheBroadcast* ParentBroadcast = ParentBroadcastWeak.Get();
	return ensure(ParentBroadcast) ? *ParentBroadcast : UAvalancheBroadcast::Get();
}

void FAvaBroadcastProfile::CopyProfiles(const FAvaBroadcastProfile& InSourceProfile, FAvaBroadcastProfile& OutTargetProfile)
{
	OutTargetProfile.Channels.Empty(InSourceProfile.Channels.Num());
	for (const FAvaOutputChannel& SourceChannel : InSourceProfile.Channels)
	{
		FAvaOutputChannel& TargetChannel = OutTargetProfile.Channels.Add_GetRef(FAvaOutputChannel(&OutTargetProfile));
		TargetChannel.SetChannelIndex(SourceChannel.GetChannelIndex());
		
		FAvaOutputChannel::DuplicateChannel(SourceChannel, TargetChannel);
	}

	// Assuming destination profile is not current yet.
	OutTargetProfile.UpdateChannels(false);
}

bool FAvaBroadcastProfile::StartChannelBroadcast()
{
	bool bBroadcastStarted = false;
	for (FAvaOutputChannel* Channel : GetChannels())
	{
		if (Channel->StartChannelBroadcast())
		{
			bBroadcastStarted = true;
		}
	}
	return bBroadcastStarted;
}

void FAvaBroadcastProfile::StopChannelBroadcast()
{
	for (FAvaOutputChannel* Channel : GetChannels())
	{
		Channel->StopChannelBroadcast();
	}
}

bool FAvaBroadcastProfile::IsBroadcastingAnyChannel() const
{
	for (const FAvaOutputChannel* Channel : GetChannels())
	{
		if (Channel->GetState() == EAvaChannelState::Live)
		{
			return true;
		}
	}
	return false;
}

bool FAvaBroadcastProfile::IsBroadcastingAllChannels() const
{
	for (const FAvaOutputChannel* Channel : GetChannels())
	{
		if (Channel->GetState() != EAvaChannelState::Live)
		{
			return false;
		}
	}
	return !GetChannels().IsEmpty();
}

bool FAvaBroadcastProfile::IsValidProfile() const
{
	return this != &FAvaBroadcastProfile::NullProfile && !ProfileName.IsNone();
}

namespace UE::AvaBroadcastProfile::Private
{
	// Since the channel index is serialized (now), we will initialize it to the
	// channel's profile index only if it hasn't been set already.
	inline void ConditionalSetChannelIndex(FAvaOutputChannel& InChannel, int32 InIndex)
	{
		if (InChannel.GetChannelIndex() == INDEX_NONE)
		{
			InChannel.SetChannelIndex(InIndex);
		}
	}
}

void FAvaBroadcastProfile::PostLoadProfile(bool bInIsProfileActive, UAvalancheBroadcast* InBroadcast)
{
	ParentBroadcastWeak = InBroadcast;
	int32 Index = 0;
	for (FAvaOutputChannel& Channel : Channels)
	{
		UE::AvaBroadcastProfile::Private::ConditionalSetChannelIndex(Channel, Index++);
		Channel.PostLoadMediaOutputs(bInIsProfileActive, this);
	}

	ResolveChannels();
}

void FAvaBroadcastProfile::UpdateChannels(bool bInIsProfileActive)
{
	int32 Index = 0;
	for (FAvaOutputChannel& Channel : Channels)
	{
		UE::AvaBroadcastProfile::Private::ConditionalSetChannelIndex(Channel, Index++);
	}

	ResolveChannels();

	// Remark: include the pinned channels when updating resources.
	for (FAvaOutputChannel* Channel : ResolvedChannels)
	{
		Channel->UpdateChannelResources(bInIsProfileActive);
	}
}

void FAvaBroadcastProfile::ResolveChannels()
{
	const UAvalancheBroadcast& Broadcast = GetBroadcast();
	ResolvedChannels.Empty(Broadcast.GetChannelNameCount());

	for (int32 ChannelIndex = 0; ChannelIndex < Broadcast.GetChannelNameCount(); ++ChannelIndex)
	{
		const FName ChannelName = Broadcast.GetChannelName(ChannelIndex);
		FAvaOutputChannel& Channel = GetChannelMutable(ChannelName);
		if (Channel.IsValidChannel())
		{
			ResolvedChannels.Add(&Channel);
		}
	}
}

FAvaOutputChannel& FAvaBroadcastProfile::AddChannel(FName InChannelName)
{
	UAvalancheBroadcast& Broadcast = GetBroadcast();

	int32 ChannelNameIndex;
	if (!InChannelName.IsNone())
	{
		// If a name is provided, added it in the broadcast channel names.
		// It may exist already, in which case the existing index is used.
		ChannelNameIndex = Broadcast.AddChannelName(InChannelName);	
	}
	else
	{
		// Determine the last name index in this profile.
		int32 LastChannelNameIndex = -1;
		for (const FAvaOutputChannel& Channel : Channels)
		{
			if (Channel.GetChannelIndex() > LastChannelNameIndex)
			{
				LastChannelNameIndex = Channel.GetChannelIndex();
			}
		}
		
		ChannelNameIndex = LastChannelNameIndex + 1;

		// Add the name to the broadcast if needed.
		Broadcast.GetOrAddChannelName(ChannelNameIndex);
	}

	FAvaOutputChannel& Channel = Channels.Add_GetRef(FAvaOutputChannel(this));
	Channel.SetChannelIndex(ChannelNameIndex);
	Channel.UpdateChannelResources(Broadcast.GetCurrentProfileName() == GetName());
	
	Broadcast.UpdateChannelNames();
	ResolveChannels();

	Broadcast.QueueNotifyChange(EAvaBroadcastChange::ChannelGrid);
	Broadcast.GetOnChannelsListChanged().Broadcast(*this);
	
	return Channel;
}

bool FAvaBroadcastProfile::RemoveChannel(FName InChannelName)
{
	UAvalancheBroadcast& Broadcast = GetBroadcast();
	
	const int32 ChannelIndex = GetLocalChannelIndexInProfile(InChannelName);

	if (ChannelIndex == INDEX_NONE)
	{
		UE_LOG(LogAvaBroadcast, Error,
			TEXT("Can't remove channel \"%s\" from profile \"%s\", not found in profile."),
			*InChannelName.ToString(), *ProfileName.ToString());
		return false;
	}

	if (!Channels.IsValidIndex(ChannelIndex))
	{
		UE_LOG(LogAvaBroadcast, Error,
			TEXT("Can't remove channel \"%s\" (index %d) from profile \"%s\": invalid index."),
			*InChannelName.ToString(), ChannelIndex, *ProfileName.ToString());
		return false;
	}
	
	Channels.RemoveAt(ChannelIndex);
	Broadcast.UpdateChannelNames();	// Will update channel name indices in all profiles.

	// Handle removing a pinned channel.
	if (Broadcast.GetPinnedChannelProfileName(InChannelName) == GetName())
	{
		Broadcast.UnpinChannel(InChannelName);
		Broadcast.RebuildProfiles();	// Channel names must be updated before calling this.
	}
	else
	{
		// If the channel is not pinned, resolve only this profile's channels.
		ResolveChannels();
	}

	Broadcast.QueueNotifyChange(EAvaBroadcastChange::ChannelGrid);
	Broadcast.GetOnChannelsListChanged().Broadcast(*this);
	return true;
}

int32 FAvaBroadcastProfile::GetLocalChannelIndexInProfile(FName InChannelName) const
{
	return Channels.IndexOfByPredicate([InChannelName](const FAvaOutputChannel& InChannel)
	{
		return InChannel.GetChannelName() == InChannelName;
	});
}

int32 FAvaBroadcastProfile::GetChannelIndexInProfile(FName InChannelName) const
{
	return ResolvedChannels.IndexOfByPredicate([InChannelName](const FAvaOutputChannel* InChannel)
	{
		return InChannel->GetChannelName() == InChannelName;
	});
}

const FAvaOutputChannel& FAvaBroadcastProfile::GetLocalChannel(FName InChannelName) const
{
	const int32 ChannelIndex = GetLocalChannelIndexInProfile(InChannelName);
	
	if (Channels.IsValidIndex(ChannelIndex))
	{
		return Channels[ChannelIndex];
	}
	
	return FAvaOutputChannel::NullChannel;
}

FAvaOutputChannel& FAvaBroadcastProfile::GetLocalChannelMutable(FName InChannelName)
{
	const int32 ChannelIndex = GetLocalChannelIndexInProfile(InChannelName);
	
	if (Channels.IsValidIndex(ChannelIndex))
	{
		return Channels[ChannelIndex];
	}
	
	return FAvaOutputChannel::NullChannel;
}

const FAvaOutputChannel& FAvaBroadcastProfile::GetChannel(FName InChannelName) const
{
	UAvalancheBroadcast& Broadcast = GetBroadcast();
	const FName PinnedChannelProfileName = Broadcast.GetPinnedChannelProfileName(InChannelName);
	if (PinnedChannelProfileName != NAME_None)
	{
		return Broadcast.GetProfile(PinnedChannelProfileName).GetLocalChannel(InChannelName);
	}	
	return GetLocalChannel(InChannelName); 
}

FAvaOutputChannel& FAvaBroadcastProfile::GetChannelMutable(FName InChannelName)
{
	UAvalancheBroadcast& Broadcast = GetBroadcast();
	const FName PinnedChannelProfileName = Broadcast.GetPinnedChannelProfileName(InChannelName);
	if (PinnedChannelProfileName != NAME_None)
	{
		return Broadcast.GetProfile(PinnedChannelProfileName).GetLocalChannelMutable(InChannelName);
	}	
	return GetLocalChannelMutable(InChannelName);
}

FAvaOutputChannel& FAvaBroadcastProfile::GetOrAddChannel(FName InChannelName)
{
	FAvaOutputChannel& Channel = GetChannelMutable(InChannelName);
	return Channel.IsValidChannel() ? Channel : AddChannel(InChannelName);
}

UMediaOutput* FAvaBroadcastProfile::AddChannelMediaOutput(FName InChannelName, const UClass* InMediaOutputClass, const FAvaMediaOutputInfo& InOutputInfo)
{
	FAvaOutputChannel& Channel = GetChannelMutable(InChannelName);
	if (Channel.IsValidChannel())
	{
		return Channel.AddMediaOutput(InMediaOutputClass, InOutputInfo);
	}
	return nullptr;
}

int32 FAvaBroadcastProfile::RemoveChannelMediaOutputs(FName InChannelName, const TArray<UMediaOutput*>& InMediaOutputs)
{
	FAvaOutputChannel& Channel = GetChannelMutable(InChannelName);
	if (Channel.IsValidChannel())
	{
		int32 RemovedCount = 0;
		for (UMediaOutput* const MediaOutput : InMediaOutputs)
		{
			RemovedCount += Channel.RemoveMediaOutput(MediaOutput);
		}		
		return RemovedCount;
	}
	return 0;
}

#undef LOCTEXT_NAMESPACE