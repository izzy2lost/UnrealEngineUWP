// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "AvaMediaPlayableGroup.h"
#include "AvalancheRemoteControlValues.h"
#include "Nodes/Events/Actions/AvalancheAnimations.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

#include "AvalanchePlayable.generated.h"

class IAvaSceneInterface;
class UAvaMediaPlayableGroup;
class UAvaMediaPlayableGroupManager;
class UAvaSequence;
class UAvaSequencePlayer;
class UAvalanchePlayableTransition;
class URemoteControlPreset;
struct FAvaSoftAssetPtr;
struct FAvaInstancePlaySettings;
struct FAvalancheRemoteControlValues;

AVALANCHEMEDIA_API DECLARE_LOG_CATEGORY_EXTERN(LogAvalanchePlayable, Log, All);

/** Option flags for the EndPlay function. */
UENUM()
enum class EAvalanchePlayableEndPlayOptions : uint8
{
	None = 0,
	/** End play world if no more assets are playing. */
	ConditionalEndPlayWorld = 1 << 0,
	/** Perform the request immediately instead of waiting on the next tick. */
	ForceImmediate = 1 << 1,
};
ENUM_CLASS_FLAGS(EAvalanchePlayableEndPlayOptions);

enum class EAvalanchePlayableCommandResult : uint8
{
	/** The command was executed successfully. */
	Executed = 0,
	/** The command failed and should be discarded. */
	ErrorDiscard = 1,
	/** The command couldn't be executed but should be kept and attempted again. */
	KeepPending = 2
};

/*
 * Base class for an avalanche playable.
 *
 * A playable (a.k.a. graphic or page) is the basic element that can be rendered
 * and controlled through the animations and remote control.
 * 
 * Design goal:
 *
 * The design goal is to abstract the implementation of a playable.
 * So far we have 2 implementations:
 * - Avalanche Blueprint that requires to be in it's own game instance/world.
 * - Level Streaming that can be streamed with other levels in the same game instance.
 *
 * To support multiple playable in the same channel/output, there are 2 ways:
 * - rendering in the same world which can only be done with the level streaming playables.
 * - compositing different renders. This is not yet supported, but the playable abstraction should help.
 *
 * Future Work:
 * There is also another abstraction desired which is the "remote" playable. The AvalanchePlayback
 * has a lot of logic to manage remote replication of playable commands. It would probably
 * be cleaner and more reusable to have a RemotePlayable class that implements all those instead. 
 */
UCLASS(Abstract, NotBlueprintable, BlueprintType)	// Todo: review class specifier
class AVALANCHEMEDIA_API UAvalanchePlayable : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Playable creation information contains the necessary information to
	 * create an instance of a playable and it's playable group.
	 */
	struct FPlayableCreationInfo
	{
		/** Container for shared playable groups. */
		UAvaMediaPlayableGroupManager* PlayableGroupManager = nullptr;
		/** Information about the asset type and path. */
		FAvaSoftAssetPtr SourceAsset;
		/** Channel name this playable will be instanced in. */
		FName ChannelName;
		/** Provided Playable Group, if not provided, the channel name will be used. */
		UAvaMediaPlayableGroup* PlayableGroup = nullptr;
	};
	
	/**
	 * Factory method for the playables.
	 * This will create the appropriate playable object and initialize it (calling InitPlayable).
	 * This will setup the playable group appropriately, but will not load the asset.
	 */
	static UAvalanchePlayable* Create(UObject* InOuter, const FPlayableCreationInfo& InPlayableInfo);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSequenceEvent, UAvalanchePlayable*, const FName& /*InSequenceName*/, EAvalanchePlayableSequenceEventType);
	static FOnSequenceEvent& OnSequenceEvent() { return OnSequenceEventDelegate; }

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTransitionEvent, UAvalanchePlayable*, UAvalanchePlayableTransition*, EAvalanchePlayableTransitionEventFlags);
	static FOnTransitionEvent& OnTransitionEvent() { return OnTransitionEventDelegate; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayableStatusChanged, UAvalanchePlayable*);
	FOnPlayableStatusChanged& OnPlayableStatusChanged() { return OnPlayableStatusChangedDelegate; }
	
	virtual bool LoadAsset(const FAvaSoftAssetPtr& InAvalancheSourceAsset, bool bInInitiallyVisible) { return false;}
	virtual bool UnloadAsset() {return false;}
	virtual const FSoftObjectPath& GetSourceAssetPath() const;
	virtual EAvalanchePlayableStatus GetPlayableStatus() const { return EAvalanchePlayableStatus::Unknown;}
	virtual IAvaSceneInterface* GetSceneInterface() const {return nullptr;}
	virtual EAvalanchePlayableCommandResult ExecuteAnimationCommand(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings);
	virtual EAvalanchePlayableCommandResult UpdateRemoteControlCommand(const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues);
	virtual bool ApplyCamera() { return false; }
	virtual bool IsRemoteProxy() const { return false; }
	virtual bool GetShouldBeVisible() const { return true; }
	virtual void SetShouldBeVisible(bool bInShouldBeVisible) {}
	
	/**
	 * @brief Ensures the given asset is playing (visible) with the given parameters.
	 * @param InWorldPlaySettings World settings (render target) to render this asset with.
	 * @remark This doesn't trigger the animations.
	 */
	void BeginPlay(const FAvaInstancePlaySettings& InWorldPlaySettings);

	/**
	 * @brief Make this asset not play anymore (will end up hidden)
	 */
	void EndPlay(EAvalanchePlayableEndPlayOptions InOptions);

	bool IsPlaying() const { return bIsPlaying; }
	
	UAvaMediaPlayableGroup* GetPlayableGroup() const { return PlayableGroup; }

	UWorld* GetPlayWorld() const { return PlayableGroup ? PlayableGroup->GetPlayWorld() : nullptr; }

	bool HasSequence(const UAvaSequence* InSequence) const;

	virtual void SetInstanceId(const FGuid& InInstanceId ) { InstanceId = InInstanceId; }
	const FGuid& GetInstanceId() const { return InstanceId; }

	virtual void SetUserData(const FString& InUserData) { UserData = InUserData; }
	const FString& GetUserData() const { return UserData; }

	const FAvalancheRemoteControlValues& GetLatestRemoteControlValues() const
	{
		return LatestRemoteControlValues.IsValid() ? *LatestRemoteControlValues : FAvalancheRemoteControlValues::GetDefaultEmpty();
	}

protected:
	/**
	 * @brief Performs derived initialization of the playable. Does not load the asset.
	 * @remark Playable Group is setup by the derived classes.
	 * @param InPlayableInfo Necessary information to setup the playable.
	 * @return true if the playable group and derived initialization was successful.
	 */
	virtual bool InitPlayable(const FPlayableCreationInfo& InPlayableInfo);

	/** Called by BeginPlay for derived classes implementation. */
	virtual void OnPlay() {}

	/** Called by EndPlay for derived classes implementation. */
	virtual void OnEndPlay() {}

	void HandleOnSequenceStarted(UAvaSequencePlayer* InSequencePlayer, UAvaSequence* InSequence);
	void HandleOnSequenceFinished(UAvaSequencePlayer* InSequencePlayer, UAvaSequence* InSequence);

	static UAvalanchePlayable* CreateLocalPlayable(UObject* InOuter, const FPlayableCreationInfo& InPlayableInfo);
	static UAvalanchePlayable* CreateRemoteProxyPlayable(UObject* InOuter, const FPlayableCreationInfo& InPlayableInfo);

protected:
	/**
	 * Playable "instancing container" group.
	 * Defines the interface for what a playable can do to it's container.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAvaMediaPlayableGroup> PlayableGroup;
	
	bool bIsPlaying = false;

	/**
	 * Unique identifier for this playable instance.
	 * This is used for the playback client and server's playable replication. 
	 */
	FGuid InstanceId;

	/**
	 * User data that is replicated to and from the playback server.
	 * This can be used to transport additional information for this playable.
	 */
	FString UserData;

	TSharedPtr<FAvalancheRemoteControlValues> LatestRemoteControlValues;

	static FOnSequenceEvent OnSequenceEventDelegate;
	static FOnTransitionEvent OnTransitionEventDelegate;
	FOnPlayableStatusChanged OnPlayableStatusChangedDelegate;
};
