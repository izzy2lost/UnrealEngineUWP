// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/AvalancheGameInstance.h"
#include "Playback/IAvaPlayableVisibilityConstraint.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

#include "AvaMediaPlayableGroup.generated.h"

class UAvaMediaPlayableGroupManager;
class UAvalanchePlayable;
class UAvalanchePlayableTransition;
class UTextureRenderTarget2D;

/**
 * This defines a game instance playable group.
 * It tracks and manage the game instance and playables state.
 *
 * The design goal of this class is to remove all the playback management
 * from UAvalancheGameInstance and move it to the playable framework. This
 * should allow us to hook the playback framework to any game instance, including PIE
 * so it can work with any work flow (editor, PIE, game, nDisplay, etc).
 *
 * Also, ideally, the playable class itself should be "game instance" agnostic
 * and do all it's bidding on it's container through this class.
 */
UCLASS()
class AVALANCHEMEDIA_API UAvaMediaPlayableGroup : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * PlayableGroup creation information contains the necessary information to
	 * create an instance of a playable group.
	 */
	struct FPlayableGroupCreationInfo
	{
		/** Container for shared playable groups. */
		UAvaMediaPlayableGroupManager* PlayableGroupManager = nullptr;
		/** Source asset path. */
		FSoftObjectPath SourceAssetPath;
		/** Channel name this playable will be instanced in. */
		FName ChannelName;
		/** Indicate if the group is for remote proxy playables. */
		bool bIsRemoteProxy = false;
		/** Indicate if the group is shared. */
		bool bIsSharedGroup = false;
	};
	
	static UAvaMediaPlayableGroup* MakePlayableGroup(UObject* InOuter, const FPlayableGroupCreationInfo& InPlayableGroupInfo);

	/** Register a playable to this group when it is created. */
	void RegisterPlayable(UAvalanchePlayable* InPlayable);

	/** Unregister a playable when it is about to be deleted. */
	void UnregisterPlayable(UAvalanchePlayable* InPlayable);

	/** Returns true if there are any valid registered playables. */
	bool HasPlayables() const;

	/** Returns true if there are any valid registered playables that are currently playing. */
	bool HasPlayingPlayables() const;

	/** Finds all the playables that are instances of the given source asset. */
	void FindPlayablesBySourceAssetPath(const FSoftObjectPath& InSourceAssetPath, TArray<UAvalanchePlayable*>& OutFoundPlayables) const;

	void RegisterPlayableTransition(UAvalanchePlayableTransition* InPlayableTransition);
	
	void UnregisterPlayableTransition(UAvalanchePlayableTransition* InPlayableTransition);

	/** Tick transitions that have been registered. Returns the number of transitions that where ticked. */
	void TickTransitions(double InDeltaSeconds);

	bool HasTransitions() const;

	/**
	 * Creates the game instance's world if it wasn't already.
	 * @return true if the world was created. false if nothing was done.
	 */
	bool ConditionalCreateWorld();

	/**
	 * Begin playing the game instance's world if it wasn't already.
	 * @return true if the BeginPlay was done (i.e. on the state transition only), false otherwise.
	 */
	virtual bool ConditionalBeginPlay(const FAvalancheInstancePlaySettings& InWorldPlaySettings);

	virtual void RequestEndPlayWorld(bool bInForceImmediate);

	/** Keep track of the last playable that applied it's camera in the viewport/controller. */
	void SetLastAppliedCameraPlayable(UAvalanchePlayable* InPlayable);
	
	/** Search for the first remaining playing playable and use it's camera. */
	bool UpdateCameraSetup();

	bool IsWorldPlaying() const { return GameInstance ? GameInstance->IsWorldPlaying() : false;}

	bool IsRenderTargetReady() const { return GameInstance ? GameInstance->IsRenderTargetReady() : false;}

	/**
	 * Current logic for the render target, use the game instance's if there, fallback to internal one if not.
	 */
	UTextureRenderTarget2D* GetRenderTarget() const { return GameInstance ? GameInstance->GetRenderTarget() : RenderTarget.Get(); }

	/** Return a vanilla game instance, we want to eventually support any game instance. */
	UGameInstance* GetGameInstance() const { return GameInstance;}
	
	UWorld* GetPlayWorld() const { return GameInstance ? GameInstance->GetPlayWorld() : nullptr; }
	
	/**
	 * Unloads the game instance's world if no more playables are loaded.
	 * @return true if the world was unloaded. false if nothing was done.
	 */ 
	bool ConditionalRequestUnloadWorld(bool bForceImmediate);

	/**
	 * @brief Queue a camera cut for the next rendered frame.
	 */
	void QueueCameraCut();

	/**
	 * @brief Notify the playable group that a playable is loading an asset.
	 * @param InPlayable Source of the event.
	 */
	void NotifyLevelStreaming(UAvalanchePlayable* InPlayable);

	UAvaMediaPlayableGroupManager* GetPlayableGroupManager() const { return ParentPlayableGroupManagerWeak.Get(); }

	void RegisterVisibilityConstraint(const TWeakInterfacePtr<IAvaPlayableVisibilityConstraint>& InVisibilityConstraint);
	void UnregisterVisibilityConstraint(const IAvaPlayableVisibilityConstraint* InVisibilityConstraint);

	void RequestSetVisibility(UAvalanchePlayable* InPlayable, bool bInShouldBeVisible);

protected:
	bool IsVisibilityConstrained(const UAvalanchePlayable* InPlayable) const;

	void OnPlayableStatusChanged(UAvalanchePlayable* InPlayable);
	
	void ConditionalRegisterWorldDelegates(UWorld* InWorld);
	void UnregisterWorldDelegates(UWorld* InWorld);

	bool DisplayLoadedAssets(FText& OutText, FLinearColor& OutColor);
	bool DisplayPlayingAssets(FText& OutText, FLinearColor& OutColor);
	bool DisplayTransitions(FText& OutText, FLinearColor& OutColor);

	static UPackage* MakeGameInstancePackage(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName);
	static UPackage* MakeSharedInstancePackage(const FName& InChannelName);
	static UPackage* MakeInstancePackage(const FString& InInstancePackageName);

public:	
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;	// Optional
	
	UPROPERTY(Transient)
	TObjectPtr<UAvalancheGameInstance> GameInstance;

	UPROPERTY(Transient)
	TObjectPtr<UPackage> GameInstancePackage;

protected:
	/** PlayableGroup Manager handling this playable group. */
	TWeakObjectPtr<UAvaMediaPlayableGroupManager> ParentPlayableGroupManagerWeak;
	
	/** List of playables for this group. */
	TSet<TObjectKey<UAvalanchePlayable>> Playables;

	/** Last playable that applied a camera. */
	TWeakObjectPtr<UAvalanchePlayable> LastAppliedCameraPlayableWeak;

	/** Set of registered playable transitions for this group. Remark: used for ticking. */
	TSet<TObjectKey<UAvalanchePlayableTransition>> PlayableTransitions;

	/** If transitions are added or removed while ticking, we need to protect transition iterator. */
	bool bIsTickingTransitions = false;

	/** Set of transitions to remove accumulated during transition ticking. */
	TSet<TObjectKey<UAvalanchePlayableTransition>> PlayableTransitionsToRemove;

	/** Set of transitions to add accumulated during transition ticking. */
	TSet<TObjectKey<UAvalanchePlayableTransition>> PlayableTransitionsToAdd;

	/**
	 * Since the UViewportStatsSubsystem delegation mechanism does not allow us to verify
	 * if it is bound to this world or another one. We need this auxiliary binding
	 * tracking to compensate.
	 */
	TWeakObjectPtr<UWorld> LastWorldBoundToDisplayDelegates;

	/** UViewportStatsSubsystem display delegate indices. */
	TArray<int32> DisplayDelegateIndices;

	struct FVisibilityRequest
	{
		TWeakObjectPtr<UAvalanchePlayable> PlayableWeak;
		bool bShouldBeVisible;

		void Execute(const UAvaMediaPlayableGroup* InPlayableGroup) const;
	};
	
	TArray<FVisibilityRequest> VisibilityRequests;

	TArray<TWeakInterfacePtr<IAvaPlayableVisibilityConstraint>> VisibilityConstraints;
};

/**
 *	Remote Proxy Playable Group doesn't have a game instance.
 */
UCLASS()
class AVALANCHEMEDIA_API UAvaMediaRemoteProxyPlayableGroup : public UAvaMediaPlayableGroup
{
	GENERATED_BODY()
	
public:
	virtual bool ConditionalBeginPlay(const FAvalancheInstancePlaySettings& InWorldPlaySettings) override;
	virtual void RequestEndPlayWorld(bool bInForceImmediate) override;
	
	bool bIsPlaying = false;
};