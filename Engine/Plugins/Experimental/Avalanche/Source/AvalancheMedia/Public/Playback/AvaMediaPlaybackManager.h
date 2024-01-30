// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/StrongObjectPtr.h"
#include "Playback/AvalanchePlayback.h"
#include "AvaMediaPlaybackManager.generated.h"

class FAvaMediaPlaybackManager;
class FAvaMediaPlaybackSourceAssetEntry;
class IAvaMediaSyncProvider;
class UAvaMediaPlayableGroupManager;
class UAvaMediaPlaybackTransition;
class UAvalancheBlueprint;
struct FAnimPlaySettings;	// private


/** Handle to a playback instance for recycling. */
class AVALANCHEMEDIA_API FAvaMediaPlaybackInstance : public FGCObject
{
public:
	FAvaMediaPlaybackInstance() = default;
	FAvaMediaPlaybackInstance(const FGuid& InInstanceId, const FSoftObjectPath& InSourcePath, const FString& InChannelName, UAvalanchePlayback* InPlayback);
	virtual ~FAvaMediaPlaybackInstance() override;

	//~ Begin FGCObject
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject

	/**
	 * This is used by the server to track the client's id.
	 * It can also be used when reconciling the client state from the server.
	 */
	void SetInstanceId(const FGuid& InInstanceId);

	/**
	 * Set the instance's user data. This is propagate to the server and can be
	 * used when reconciling the client state.
	 */
	void SetInstanceUserData(const FString& InUserData);
	
	const FGuid& GetInstanceId() const { return InstanceId; }
	const FString& GetInstanceUserData() const { return InstanceUserData;}
	const FString& GetChannelName() const { return ChannelName; }
	const FName& GetChannelFName() const { return ChannelFName; }
	const FSoftObjectPath& GetSourcePath() const { return SourcePath; }
	UAvalanchePlayback* GetPlayback() const { return Playback; }
	EAvaMediaPlaybackStatus GetStatus() const { return Status; }
	bool IsPlaying() const { return Playback ? Playback->IsPlaying() : false;}

	bool UpdateStatus();
	void SetStatus(EAvaMediaPlaybackStatus InStatus) { Status = InStatus; }

	void Unload();
	void Recycle();

	TSharedPtr<FAvaMediaPlaybackManager> GetManager() const;

private:
	void OnPlayableCreated(UAvalanchePlayback* InPlayback, UAvalanchePlayable* InPlayable);
	
private:
	/** If the cache slot becomes invalid, it means this instance will be discarded instead of being recycled. */
	TWeakPtr<FAvaMediaPlaybackSourceAssetEntry> AssetEntryWeak;

	FGuid InstanceId;
	FString ChannelName;
	FName ChannelFName;
	FSoftObjectPath SourcePath;
	TObjectPtr<UAvalanchePlayback> Playback;
	EAvaMediaPlaybackStatus Status = EAvaMediaPlaybackStatus::Unknown;
	FString InstanceUserData;

	friend class FAvaMediaPlaybackManager;
};

/**
 * For each playback instance, a source asset entry is kept track of to allow the entry to be
 * invalidated and prevent the instance to be recycled.
 */
class AVALANCHEMEDIA_API FAvaMediaPlaybackSourceAssetEntry
{
public:
	FAvaMediaPlaybackSourceAssetEntry(const TSharedPtr<FAvaMediaPlaybackManager>& InParentManager) : ParentManagerWeak(InParentManager.ToWeakPtr()) {}
	~FAvaMediaPlaybackSourceAssetEntry() = default;

	TSharedPtr<FAvaMediaPlaybackInstance> AcquirePlaybackInstance(const FString& InChannelName)
	{
		TSharedPtr<FAvaMediaPlaybackInstance> AcquiredInstance;
		const int32 InstanceIndex = AvailableInstances.IndexOfByPredicate([InChannelName](const TSharedPtr<FAvaMediaPlaybackInstance>& InInstance)
		{
			return InInstance && InInstance->GetChannelName() == InChannelName;
		});

		if (InstanceIndex != INDEX_NONE)
		{
			AcquiredInstance = AvailableInstances[InstanceIndex];
			AvailableInstances.RemoveAtSwap(InstanceIndex, 1);
			UsedInstances.Add(AcquiredInstance.ToWeakPtr());
		}
		return AcquiredInstance;
	}

	TSharedPtr<FAvaMediaPlaybackInstance> FindPlaybackInstance(const FGuid& InInstanceId,
		bool bInAvailableInstances = true, bool bInUsedInstances = true) const;
	
	TSharedPtr<FAvaMediaPlaybackInstance> FindPlaybackInstanceForChannel(const FString& InChannelName,
		bool bInAvailableInstances = true, bool bInUsedInstances = true) const;

	TSharedPtr<FAvaMediaPlaybackInstance> FindPlaybackInstanceByPredicate(TFunctionRef<bool(const FAvaMediaPlaybackInstance& /*InPlaybackInstance*/)> InPredicate,
		bool bInAvailableInstances = true, bool bInUsedInstances = true) const;
	
	void ForAllPlaybackInstances(TFunctionRef<void(FAvaMediaPlaybackInstance& /*InPlaybackInstance*/)> InFunction,
		bool bInAvailableInstances = true, bool bInUsedInstances = true);

	TSharedPtr<FAvaMediaPlaybackManager> GetManager() const { return ParentManagerWeak.Pin(); }

	void DiscardInstance(FAvaMediaPlaybackInstance* InInstanceToRemove);
	void RecycleInstance(FAvaMediaPlaybackInstance* InInstanceToRecycle);

private:
	/** Keeping a weak reference to the manager to recycle the instance. */
	TWeakPtr<FAvaMediaPlaybackManager> ParentManagerWeak;
	
	TArray<TSharedPtr<FAvaMediaPlaybackInstance>> AvailableInstances;
	TArray<TWeakPtr<FAvaMediaPlaybackInstance>> UsedInstances;

	friend class FAvaMediaPlaybackManager;
};

/** Flags indicating what changed in the package event. */
UENUM()
enum class EAvaMediaPlaybackPackageEventFlags : uint8
{
	None			= 0,
	External		= 1 << 0,
	Saved			= 1 << 1,
	AssetDeleted	= 1 << 2,
	All				= 0xFF
};
ENUM_CLASS_FLAGS(EAvaMediaPlaybackPackageEventFlags);

class AVALANCHEMEDIA_API FAvaMediaPlaybackManager : public TSharedFromThis<FAvaMediaPlaybackManager>
{
public:
	FAvaMediaPlaybackManager();
	virtual ~FAvaMediaPlaybackManager();

	void Tick();

	void SetEnablePlaybackCommandsBuffering(bool bInEnable) { bEnablePlaybackCommandsBuffering = bInEnable;}

	UAvaMediaPlayableGroupManager* GetPlayableGroupManager() const { return PlayableGroupManager.Get(); }

	/** Acquire a cached playback instance. Will return null if none available in the cache. */
	TSharedPtr<FAvaMediaPlaybackInstance> AcquirePlaybackInstance(const FSoftObjectPath& InAssetPath, const FString& InChannelName) const;
	
	/** Load a new playback instance. */
	TSharedPtr<FAvaMediaPlaybackInstance> LoadPlaybackInstance(const FSoftObjectPath& InAssetPath, const FString& InChannelName);

	/** Attempts to acquire existing instance. If none available, will load a new one. */  
	TSharedPtr<FAvaMediaPlaybackInstance> AcquireOrLoadPlaybackInstance(const FSoftObjectPath& InAssetPath, const FString& InChannelName);

	/**
	 * Finds an existing (either available or used) playback instance.
	 * This will not acquire it (i.e. if available, it will remain so).
	 */
	TSharedPtr<FAvaMediaPlaybackInstance> FindPlaybackInstance(const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName) const;

	/**
	 * Unload and discard any available (i.e. not used) instances for this asset/channel entry.
	 * If specified channel name is empty, it will discard all instances of the asset.
	 */
	bool UnloadPlaybackInstances(const FSoftObjectPath& InAssetPath, const FString& InChannelName);
	
	void ForAllPlaybackInstances(TFunctionRef<void(FAvaMediaPlaybackInstance& /*InPlaybackInstance*/)> InFunction);
	
	/**
	 *	Determines the local status of the given asset on this local instance of the playback manager.
	 */
	EAvaMediaPlaybackAssetStatus GetLocalAssetStatus(const FName& InPackageName);
	
	/**
	 * Invalidates the cached local asset status.
	 */
	void InvalidateCachedLocalAssetStatus(const FName& InPackageName);

	/**
	 * Utility function to determine if an asset is locally available.
	 * Remark: Determining the presence of dependencies is somewhat unreliable i.e.
	 * an asset can playback fine even with some dependencies missing and it is hard to figure it out.
	 * Because of that, for now, we consider the asset available even if it is missing some dependencies.
	 */
	bool IsLocalAssetAvailable(const FName& InPackageName)
	{
		const EAvaMediaPlaybackAssetStatus LocalAssetStatus = GetLocalAssetStatus(InPackageName);
		return (LocalAssetStatus == EAvaMediaPlaybackAssetStatus::Available || LocalAssetStatus == EAvaMediaPlaybackAssetStatus::MissingDependencies);
	}

	bool IsLocalAssetAvailable(const FSoftObjectPath& InAssetPath)
	{
		return IsLocalAssetAvailable(InAssetPath.GetLongPackageFName());
	}

	/** Utility function to determine the playback status of an unloaded asset. */
	EAvaMediaPlaybackStatus GetUnloadedPlaybackStatus(const FSoftObjectPath& InAssetPath)
	{
		// When the playback entry is unloaded, we rely on the local asset status to determine the playback status.
		// Note: since there is now an independent asset status, we could remove all the playback states related to the asset.
		return IsLocalAssetAvailable(InAssetPath) ? EAvaMediaPlaybackStatus::Available : EAvaMediaPlaybackStatus::Missing;
	}

	/**
	 *	Invalidate the asset entry. All cached instances will be invalidated along with it.
	 */
	void InvalidatePlaybackAssetEntry(const FSoftObjectPath& InAssetPath);
	
	UAvalanchePlayback* LoadPlaybackObject(const FSoftObjectPath& InAssetPath, const FString& InChannelName) const;
	UAvalanchePlayback* BuildPlaybackFromBlueprint(const UAvalancheBlueprint* InBlueprint, const FString& InChannelName) const;
	UAvalanchePlayback* BuildPlaybackFromWorld(const TSoftObjectPtr<UWorld>& InWorld, const FString& InChannelName) const;
	
	/**
	 * Stops all currently playing playback objects.
	 * @param bInUnload if true, will also unload the objects.
	 * @return Returns the list of all source blueprint assets that where stopped.
	 **/
	TArray<FSoftObjectPath> StopAllPlaybacks(bool bInUnload);

	bool PushAnimationCommand(const FGuid& InInstanceId, const FSoftObjectPath& InSourcePath, const FString& InChannelName, EAvaMediaAnimAction InAction, const FAnimPlaySettings& InAnimSettings);
	bool PushRemoteControlCommand(const FGuid& InInstanceId, const FSoftObjectPath& InSourcePath, const FString& InChannelName, const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues);
	bool PushPlaybackTransitionStartCommand(UAvaMediaPlaybackTransition* InTransitionToStart);

	/** This is used on the playback server to apply any pending commands to a playback instance. */
	void ApplyPendingCommands(UAvalanchePlayback* InPlaybackObject, const FGuid& InInstanceId, const FSoftObjectPath& InSourcePath, const FString& InChannelName);

	/**
	 *	Indicate the manager is in a shutdown sequence and will force game instances to destroy worlds right away.
	 */
	void StartShuttingDown() { bIsShuttingDown = true;}

	EAvaPlaybackStopOptions GetPlaybackStopOptions(bool bInUnload) const
	{
		EAvaPlaybackStopOptions Options = bIsShuttingDown ? EAvaPlaybackStopOptions::ForceImmediate : EAvaPlaybackStopOptions::None;
		Options |= (bInUnload || bIsShuttingDown) ? EAvaPlaybackStopOptions::Unload : EAvaPlaybackStopOptions::None;
		return Options;
	}
	
	EAvaPlaybackUnloadOptions GetPlaybackUnloadOptions() const
	{
		return bIsShuttingDown ? EAvaPlaybackUnloadOptions::ForceImmediate : EAvaPlaybackUnloadOptions::None;
	}

	/** Let the Playback manager know that a package has been modified. */
	void OnPackageModified(const FName& InPackageName, EAvaMediaPlaybackPackageEventFlags InFlags = EAvaMediaPlaybackPackageEventFlags::None);

	/**
	 *	TearDown the whole Avalanche Playback system.
	 *	
	 *	When Avalanche Playback is used within a game, we need to tear down everything
	 *	as the parent world is being teared down. The game tear down process will forcibly
	 *	mark as garbage (and GCs) all the GameInstances, including those held by the playback objects,
	 *	despite playback objects holding strong references to them. To avoid issues we preemptively
	 *	destroy all the playback objects.
	 */
	void OnParentWorldBeginTearDown();

	/**
	 * Implements a similar command to Engine::HandleStatCommand, except it
	 * will fetch avalanche's game viewport client if everything else fails.
	 */
	bool HandleStatCommand(const TArray<FString>& InArgs);

	/** Returns true if the given asset is a playback asset, i.e. either a "playable" asset or a playback graph. */
	static bool IsPlaybackAsset(const FAssetData& InAssetData);
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlaybackInstanceInvalidated, const FAvaMediaPlaybackInstance&);
	FOnPlaybackInstanceInvalidated OnPlaybackInstanceInvalidated;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlaybackInstanceStatusChanged, const FAvaMediaPlaybackInstance&);
	FOnPlaybackInstanceStatusChanged OnPlaybackInstanceStatusChanged;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnLocalPlaybackAssetRemoved, const FSoftObjectPath&);
	FOnLocalPlaybackAssetRemoved OnLocalPlaybackAssetRemoved;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBeginTick, float);
	FOnBeginTick OnBeginTick;

protected:
	// Non-copyable (because copy of CachedAssetStatusExtra is not defined)
	FAvaMediaPlaybackManager(const FAvaMediaPlaybackManager&) = delete;
	FAvaMediaPlaybackManager& operator=(const FAvaMediaPlaybackManager&) = delete;

	void OnPackageSaved(const FString& InPackageFileName, UPackage* InPackage, FObjectPostSaveContext InObjectSaveContext);
	void OnAvaSyncPackageModified(IAvaMediaSyncProvider* InAvaMediaSyncProvider, const FName& InPackageName);
	void OnAssetRemoved(const FAssetData& InAssetData);

	TSharedPtr<FAvaMediaPlaybackSourceAssetEntry> FindPlaybackAssetEntry(const FSoftObjectPath& InAssetPath) const
	{
		const TSharedPtr<FAvaMediaPlaybackSourceAssetEntry>* Existing = PlaybackAssetEntries.Find(InAssetPath);
		return Existing ? *Existing : TSharedPtr<FAvaMediaPlaybackSourceAssetEntry>();
	}
	
	TSharedPtr<FAvaMediaPlaybackSourceAssetEntry> GetPlaybackAssetEntry(const FSoftObjectPath& InAssetPath)
	{
		TSharedPtr<FAvaMediaPlaybackSourceAssetEntry>* Existing = PlaybackAssetEntries.Find(InAssetPath);
		if (Existing)
		{
			return *Existing;
		}

		TSharedPtr<FAvaMediaPlaybackSourceAssetEntry> NewAssetEntry = MakeShared<FAvaMediaPlaybackSourceAssetEntry>(SharedThis(this));
		
		PlaybackAssetEntries.Add(InAssetPath, NewAssetEntry);
		return NewAssetEntry;
	}
	
private:
	bool bIsShuttingDown = false;

	/** This is the shared pool of shared playable groups for all the playback objects. */
	TStrongObjectPtr<UAvaMediaPlayableGroupManager> PlayableGroupManager; 
	
	// TODO: Refactor this to support cache capacity with eviction (LRU) like FAvalancheManagedInstanceCache.
	TMap<FSoftObjectPath, TSharedPtr<FAvaMediaPlaybackSourceAssetEntry>> PlaybackAssetEntries;
	
	/** Cached asset status. */
	TMap<FName, EAvaMediaPlaybackAssetStatus> CachedAssetStatus;

	TArray<TWeakObjectPtr<UAvaMediaPlaybackTransition>> PendingStartTransitions;
	
	/**
	 * Enables the playback command buffering.
	 * 
	 * This is enabled on the playback server to handle the Remote control and animation
	 * commands being received/processed before the playback command itself (where the playback object
	 * is created). If the commands can't be executed because the object is not yet created the
	 * playback manager will buffer the commands and apply them to the object once it is loaded.
	 */
	bool bEnablePlaybackCommandsBuffering = false;
	
	struct FPlaybackObjectCommandBuffers
	{
		struct FAnimationCommand
		{
			EAvaMediaAnimAction AnimAction;
			FAnimPlaySettings AnimPlaySettings;
		};
		struct FRemoteControlCommand
		{
			TSharedRef<FAvalancheRemoteControlValues> Values;
		};
	
		TArray<FAnimationCommand> AnimationCommands;
		TArray<FRemoteControlCommand> RemoteControlCommands;
	};
	// Use FAvaMediaPlaybackManager::MakePlaybackKey for Map key.
	TMap<FString, FPlaybackObjectCommandBuffers> PlaybackObjectCommandBuffers;

	FString MakeCommandBufferKey(const FSoftObjectPath& InAssetPath, const FString& InChannelName) const
	{
		FString CommandBufferKey = InAssetPath.ToString();
		if (!InChannelName.IsEmpty())
		{
			CommandBufferKey += TEXT("_") + InChannelName;
		}
		return CommandBufferKey;
	}

	FString MakeCommandBufferKey(const FGuid& InInstanceId) const
	{
		return InInstanceId.ToString();
	}

	FString MakeCommandBufferKey(const FGuid& InInstanceId, const FSoftObjectPath& InAssetPath, const FString& InChannelName) const
	{
		return InInstanceId.IsValid() ? MakeCommandBufferKey(InInstanceId) : MakeCommandBufferKey(InAssetPath, InChannelName);
	}
	
	FPlaybackObjectCommandBuffers& GetOrCreatePlaybackCommandBuffers(FString&& InCommandBufferKey);
	const FPlaybackObjectCommandBuffers* GetPlaybackCommandBuffers(const FString& InCommandBufferKey) const;
};
