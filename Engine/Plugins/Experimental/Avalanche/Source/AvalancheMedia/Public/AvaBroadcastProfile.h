// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channel/AvaOutputChannel.h"
#include "AvaBroadcastProfile.generated.h"

class UAvalancheBroadcast;

USTRUCT()
struct AVALANCHEMEDIA_API FAvaBroadcastProfile
{
	GENERATED_BODY()

	friend UAvalancheBroadcast;
	
public:	
	FAvaBroadcastProfile() : FAvaBroadcastProfile(nullptr, NAME_None) {}
	FAvaBroadcastProfile(UAvalancheBroadcast* InBroadcast, FName InProfileName);

	void BeginDestroy();
	
	static FAvaBroadcastProfile NullProfile;
	
	FName GetName() const { return ProfileName; }
	UAvalancheBroadcast& GetBroadcast() const;
	
	static void CopyProfiles(const FAvaBroadcastProfile& InSourceProfile, FAvaBroadcastProfile& OutTargetProfile);
	
	bool StartChannelBroadcast();	
	void StopChannelBroadcast();

	bool IsBroadcastingAnyChannel() const;
	bool IsBroadcastingAllChannels() const;
	
	bool IsValidProfile() const;
	
	void PostLoadProfile(bool bInIsProfileActive, UAvalancheBroadcast* InParentBroadcast);
	
	void UpdateChannels(bool bInIsProfileActive);

	/**
	 * Add a channel with the given name. If NAME_None, a default name is given to the channel.
	 */
	FAvaOutputChannel& AddChannel(FName InChannelName = NAME_None);
	
	/**
	 * @brief Removes the channel with given name.
	 * @param InChannelName Name of the channel to remove.
	 * @return true if the channel was removed, false otherwise. 
	 */
	bool RemoveChannel(FName InChannelName);
	
	/**
	 * Finds the corresponding index of the channel with the given name in the current
	 * array of channels (including pinned channels) for this profile.
	 * @remark This index can be used to lookup directly in the profile's channel array.
	 * @return INDEX_NONE if not found.
	 */
	int32 GetChannelIndexInProfile(FName InChannelName) const;

	/**
	 * @brief Finds the channel (including pinned channels) with the given name.
	 * @remark	Pinned channels have priority, i.e. if the profile defines a local
	 *			channel of the given name, it will be overriden by the pinned channel. 
	 * @param InChannelName Name of the channel to find.
	 * @return reference to requested channel or invalid channel if not found.
	 */
	const FAvaOutputChannel& GetChannel(FName InChannelName) const;

	/**
	 * @brief Finds the channel (including pinned channels) with the given name.
	 * @remark	Pinned channels have priority, i.e. if the profile defines a local
	 *			channel of the given name, it will be overriden by the pinned channel. 
	 * @param InChannelName Name of the channel to find.
	 * @return reference to requested channel or invalid channel if not found.
	 */
	FAvaOutputChannel& GetChannelMutable(FName InChannelName);

	/**
	 * @brief Finds the existing channel with given name, or create it if missing.
	 * @param InChannelName Name of the channel to find or create.
	 * @return Reference to the requested or created channel.
	 */
	FAvaOutputChannel& GetOrAddChannel(FName InChannelName);
	
	const TArray<FAvaOutputChannel*>& GetChannels() const { return ResolvedChannels; }
	TArray<FAvaOutputChannel*>& GetChannels() { return ResolvedChannels; }
	
	UMediaOutput* AddChannelMediaOutput(FName InChannelName, const UClass* InMediaOutputClass, const FAvaMediaOutputInfo& InOutputInfo);
	int32 RemoveChannelMediaOutputs(FName InChannelName, const TArray<UMediaOutput*>& InMediaOutputs);

protected:
	/**
	 * Finds the corresponding index of the channel with the given name in the current
	 * array of local channels (not including pinned channels) for this profile.
	 * @remark This index can be used to lookup directly in the profile's channel array.
	 * @return INDEX_NONE if not found.
	 */
	int32 GetLocalChannelIndexInProfile(FName InChannelName) const;

	/**
	 * @brief Finds the local channel (not including pinned channels) with the given name.
	 * @param InChannelName Name of the channel to find.
	 * @return reference to requested channel or invalid channel if not found.
	 */
	const FAvaOutputChannel& GetLocalChannel(FName InChannelName) const;

	/**
	 * @brief Finds the local channel (not including pinned channels) with the given name.
	 * @param InChannelName Name of the channel to find.
	 * @return reference to requested channel or invalid channel if not found.
	 */
	FAvaOutputChannel& GetLocalChannelMutable(FName InChannelName);
	
	const TArray<FAvaOutputChannel>& GetLocalChannels() const { return Channels; }
	TArray<FAvaOutputChannel>& GetLocalChannels() { return Channels; }

	/** Resolve pinned channels. */
	void ResolveChannels();
	
protected:
	TWeakObjectPtr<UAvalancheBroadcast> ParentBroadcastWeak;

	UPROPERTY(Transient)
	FName ProfileName;
	
	UPROPERTY()
	TArray<FAvaOutputChannel> Channels;

	/** This array contains all resolved channels, including pinned ones from other profiles. */
	TArray<FAvaOutputChannel*> ResolvedChannels;
};

