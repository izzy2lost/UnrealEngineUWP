// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "AvalancheRemoteControlValues.h"
#include "Nodes/AvaPlaybackNodePlayer.h"
#include "Nodes/AvaPlaybackNodeRoot.h"
#include "Nodes/Events/Actions/AvalancheAnimations.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"

#include "AvalanchePlayback.generated.h"

AVALANCHEMEDIA_API DECLARE_LOG_CATEGORY_EXTERN(LogAvalanchePlayback, Log, All);

class FAvaMediaPlaybackManager;
class IAvaPlaybackGraphEditor;
class UAvaMediaPlayableGroupManager;
class UAvaPlaybackNode;
class UAvaPlaybackNodeRoot;
class UAvalancheBroadcast;
class UAvaGameInstance;
class UAvalanchePlayable;
class UAvalanchePlayableTransition;
class UEdGraph;
class UTextureRenderTarget2D;
struct FAnimPlaySettings;
struct FAvaOutputChannel;

UENUM()
enum class EAvaPlaybackStopOptions : uint8
{
	/**
	 * Default option allows for deferred execution of the request when it is safe to do so.
	 */
	None			= 0,
	/**
	 * Forces the execution of the request when it is called. Typically during shut down.
	 */
	ForceImmediate	= 1 << 1,
	/**
	 *	Unload from memory after being stopped.
	 */
	Unload			= 1 << 2,
	/**
	 * Default option allows for deferred execution of the request when it is safe to do so.
	 */
	Default			= None
};
ENUM_CLASS_FLAGS(EAvaPlaybackStopOptions);

UENUM()
enum class EAvaPlaybackUnloadOptions : uint8
{
	/**
	 * Default option allows for deferred execution of the request when it is safe to do so.
	 */
	None			= 0,
	/**
	 * Forces the execution of the request when it is called. Typically during shut down.
	 */
	ForceImmediate	= 1 << 1,
	/**
	 * Default option allows for deferred execution of the request when it is safe to do so.
	 */
	Default			= None
};
ENUM_CLASS_FLAGS(EAvaPlaybackUnloadOptions);

/**
 * Owns a group of playables.
 * Used to group playables per channels, but can be extended to any conceptual grouping.
 */
USTRUCT()
struct AVALANCHEMEDIA_API FAvalanchePlaybackPlayableGroup
{
	GENERATED_BODY()

	// TODO: For transition logic within the playback graph, we can't simply index the
	// playables by asset path since we may have to instance the same asset twice for a transition between pages
	// using the same template. For now, playlist does transition with multiple playback graph instances instead.
	UPROPERTY(Transient)
	TMap<FSoftObjectPath, TObjectPtr<UAvalanchePlayable>> Playables;
	
	UAvalanchePlayable* FindPlayable(const FSoftObjectPath& InSourceAssetPath) const
	{
		const TObjectPtr<UAvalanchePlayable>* Playable = Playables.Find(InSourceAssetPath);
		return Playable ? *Playable : nullptr;
	}

	bool HasPlayable(const UAvalanchePlayable* InPlayable) const
	{
		for (const TPair<FSoftObjectPath, TObjectPtr<UAvalanchePlayable>>& Playable : Playables)
		{
			if (Playable.Value == InPlayable)
			{
				return true;
			}
		}
		return false;
	}
	
	UAvalanchePlayable* GetFirstPlayable() const
	{
		for (const TPair<FSoftObjectPath, TObjectPtr<UAvalanchePlayable>>& Playable : Playables)
		{
			if (Playable.Value)
			{
				return Playable.Value;
			}
		}
		return nullptr;
	}
	
	void GetAllPlayables(TArray<UAvalanchePlayable*>& OutPlayables) const
	{
		for (const TPair<FSoftObjectPath, TObjectPtr<UAvalanchePlayable>>& Playable : Playables)
		{
			if (Playable.Value)
			{
				OutPlayables.Add(Playable.Value);
			}
		}
	}

	void ForEachPlayable(TFunctionRef<void(const UAvalanchePlayable*)> InFunction) const
	{
		for (const TPair<FSoftObjectPath, TObjectPtr<UAvalanchePlayable>>& Playable : Playables)
		{
			if (Playable.Value)
			{
				InFunction(Playable.Value);
			}
		}
	}
};

UCLASS(NotBlueprintable, BlueprintType)
class AVALANCHEMEDIA_API UAvalanchePlayback : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UAvalanchePlayback() override;
	
	UFUNCTION(BlueprintCallable, Category = "Motion Design Playback")
	void Play();
	
	UFUNCTION(BlueprintCallable, Category = "Motion Design Playback")
	void Stop(EAvaPlaybackStopOptions InStopOptions);
	
	UFUNCTION(BlueprintCallable, Category = "Motion Design Playback")
	void LoadInstances();

	/** Unload all game instance's worlds from this playback. */
	UFUNCTION(BlueprintCallable, Category = "Motion Design Playback")
	void UnloadInstances(EAvaPlaybackUnloadOptions InUnloadOptions);
	
	UFUNCTION(BlueprintCallable, Category = "Motion Design Playback")
	bool IsPlaying() const { return bIsPlaying; }

	/**
	 * Sets the parent playback manager that will handle ticking. 
	 */
	void SetPlaybackManager(const TSharedPtr<FAvaMediaPlaybackManager>& InPlaybackManager);

	bool IsManaged() const { return PlaybackManagerWeak.IsValid(); }
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlaybackStateChanged, bool);
	FOnPlaybackStateChanged OnPlaybackStateChanged;

	/** Quick access to the first playable in the playback graph. */ 
	UAvalanchePlayable* GetFirstPlayable() const;

	void GetAllPlayables(TArray<UAvalanchePlayable*>& OutPlayables) const;

	void ForEachPlayable(TFunctionRef<void(const UAvalanchePlayable*)> InFunction) const;

	/** Returns true if the given playable is part of this playback graph. */
	bool HasPlayable(const UAvalanchePlayable* InPlayable) const;
	
	UAvalanchePlayable* FindPlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName) const;
	UAvalanchePlayable* FindOrLoadPlayable(const FAvaSoftAssetPtr& InSourceAsset, const FName& InChannelName);
	void RemovePlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName);
	bool UnloadAndRemovePlayable(UAvalanchePlayable* InPlayable, const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName, bool bInForceImmediate);
	bool UnloadAndRemovePlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName, bool bInForceImmediate);

	TArray<UAvaGameInstance*> GetActiveGameInstances() const;
	
	/**
	 *	Determines if the graph contains a node with the given source avalanche blueprint.
	 *	This works even if the avalanche blueprint running.
	 */
	bool HasPlayerNodeForSourceAvalancheAsset(const FSoftObjectPath& InSourceAssetPath) const;

	void SetPlayableGroupManager(UAvaMediaPlayableGroupManager* InPlayableGroupManager);

	UAvaMediaPlayableGroupManager* GetPlayableGroupManager() const
	{
		return PlayableGroupManager ? PlayableGroupManager.Get() : GetGlobalPlayableGroupManager();
	}

	static UAvaMediaPlayableGroupManager* GetGlobalPlayableGroupManager();

	/** Resolve the channel name for a playback settings entry. */
	FName ResolveChannelName(const FName InPlaybackChannelName) const
	{
		// For a preview, route everything to the preview channel,
		// otherwise use the resolved channel (from traversing the graph)
		return IsPreviewOnly() ? GetPreviewChannelName() : InPlaybackChannelName;
	}

public:
	bool IsDryRunningGraph() const { return bIsDryRunningGraph; }
	void DryRunGraph(bool bDeferredExecution = false);

	/**
	 *	Sets the preview channel this playback object is dedicated to.
	 *	By doing this, the playback object will render in only that channel regardless
	 *	of the internal playback graph.
	 *	The specified channel may not exist in the broadcast configuration, in which case the
	 *	preview playback will fallback to a channel-less render target.
	 */
	void SetPreviewChannelName(const FName& InPreviewChannelName) { PreviewChannelName = InPreviewChannelName; }

	FName GetPreviewChannelName() const { return PreviewChannelName; }

	/**
	 * Returns true if the playback object is dedicated to a local preview. 
	 */
	bool IsPreviewOnly() const { return !PreviewChannelName.IsNone(); }

	/**
	 * Helper function to implement some of the event nodes logic.
	 * It will return the list of channel names that correspond this the indices.
	 */
	TArray<FName> GetChannelNamesForIndices(const TArray<int32>& InChannelIndices) const;

private:
	void DryRunGraphInternal();
	void RegisterTickDelegate();
	void UnregisterTickDelegate();
	void OnEndFrameTick();
	void ConditionalResolvePlaybackSettings();

public:
	void SetRootNode(UAvaPlaybackNodeRoot* InRoot);
	void AddPlayerNode(UAvaPlaybackNodePlayer* InPlayer);
	
	UAvaPlaybackNodeRoot* GetRootNode() const;
	
	void AddPlaybackNode(UAvaPlaybackNode* Node);
	void RemovePlaybackNode(UAvaPlaybackNode* Node);

	/**
	 * Stops the playable corresponding to the given asset in the given channel.
	 */
	void StopPlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName);

	/**
	 * Start streaming the asset in a game instance.
	 */
	void LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, const FName& InChannelName);
	
	virtual void Tick(float DeltaTime);
	
	void PushAnimationCommand(const FSoftObjectPath& InSourceAssetPath, const FString& InChannelName, EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimSettings);
	void PushRemoteControlValues(const FSoftObjectPath& InSourceAssetPath, const FString& InChannelName, const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues);

	void SetupPlaybackNode(UAvaPlaybackNode* InPlaybackNode, bool bSelectNewNode = true);

#if WITH_EDITOR
	const TArray<TObjectPtr<UAvaPlaybackNode>>& GetPlaybackNodes() const;

	/** Create the basic sound graph */
	void CreateGraph();

	/** Clears all nodes from the graph */
	void ClearGraph();

	void RefreshPlaybackNode(UAvaPlaybackNode* InPlaybackNode);	

	void CompilePlaybackNodesFromGraphNodes();

	/** Get the Playback EdGraph  */
	UEdGraph* GetGraph();

	/** Resets all graph data and nodes */
	void ResetGraph();

	void SetGraphEditor(TSharedPtr<IAvaPlaybackGraphEditor> InGraphEditor);

	TSharedPtr<IAvaPlaybackGraphEditor> GetGraphEditor() const;
#endif

	template<typename InPlaybackNodeType, typename = typename TEnableIf<TIsDerivedFrom<InPlaybackNodeType, UAvaPlaybackNode>::IsDerived>::Type>
	InPlaybackNodeType* ConstructPlaybackNode(TSubclassOf<InPlaybackNodeType> NodeClass = nullptr, bool bSelectNewNode = true)
	{
		//Ensure Class is Valid
		if (!NodeClass)
		{
			NodeClass = InPlaybackNodeType::StaticClass();
		}

		if (!NodeClass->IsChildOf(UAvaPlaybackNodeRoot::StaticClass()) || !IsValid(RootNode))
		{
			//Set flag to be transactional so it registers with undo system
			InPlaybackNodeType* const PlaybackNode = NewObject<InPlaybackNodeType>(this
				, NodeClass
				, NAME_None
				, RF_Transactional);

#if WITH_EDITOR
			PlaybackNodes.Add(PlaybackNode);
#endif
			
			PlaybackNode->PostAllocateNode();
			SetupPlaybackNode(PlaybackNode, bSelectNewNode);
		
			return PlaybackNode;
		}

		checkf(0, TEXT("Root Node has already been created!"));
		return nullptr;
	}

	/** Delegate called when a playable is created in this playback graph. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayableCreated, UAvalanchePlayback* /*InPlayback*/, UAvalanchePlayable* /*InPlayable*/);
	FOnPlayableCreated OnPlayableCreated;

protected:
	void ExecutePendingAnimationCommands();
	void ExecutePendingRemoteControlCommands();
	
	void OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange);
	void OnChannelBroadcastStateChanged(const FAvaOutputChannel& InChannel);

	void SetIsPlaying(bool bInIsPlaying);
	
	bool CanRefreshPlayback(const FAvaSoftAssetPtr& InSourceAsset, const FAvaOutputChannel& InChannel) const;

	bool RefreshPlayback(const FAvaSoftAssetPtr& InSourceAsset, FAvaOutputChannel& InChannel, const FName& InChannelName);
	
	bool CanRefreshPreview(const FAvaSoftAssetPtr& InSourceAsset) const;
	
	bool RefreshPreview(const FAvaSoftAssetPtr& InSourceAsset, FAvaOutputChannel& InChannel, const FName& InChannelName);

	static UTextureRenderTarget2D* UpdatePlaybackRenderTarget(const UAvalanchePlayable* InPlayable, const FAvaOutputChannel& InChannel);
	
protected:
	/**
	 * Keep track of the parent manager that handles ticking.
	 * If the playback graph is an asset (not managed), it will handle it's own ticking.
	 */
	TWeakPtr<FAvaMediaPlaybackManager> PlaybackManagerWeak;

	TMap<FName, FAvaPlaybackChannelParameters> PlaybackSettings;
	TMap<FName, FAvaPlaybackChannelParameters> PreviousPlaybackSettings;
	
	UPROPERTY(Transient)
	TObjectPtr<UAvaPlaybackNodeRoot> RootNode;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAvaPlaybackNodePlayer>> PlayerNodes;

#if WITH_EDITORONLY_DATA
	TWeakPtr<IAvaPlaybackGraphEditor> GraphEditorWeak;
	
	UPROPERTY()
	TObjectPtr<UEdGraph> EdGraph;

	/**
	 * An array of all the Playback Nodes, Only available in Editor (Playback doesn't need to know disconnected nodes,
	 * unless we support runtime changes as well).
	 */
	UPROPERTY()
	TArray<TObjectPtr<UAvaPlaybackNode>> PlaybackNodes;
#endif

	/**
	 * Playback Group Manager for this Playback object.
	 *
	 * This manager defines the scope with which this playback object's playables
	 * can share their group with. In other words, the playables of this playback
	 * (that support sharing a group) can have a shared group (for the given channel)
	 * cached in this given manager.
	 *
	 * So far, the PlayableGroupManager has the same scope as the playable manager,
	 * i.e. global (a.k.a local) or per playback server.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAvaMediaPlayableGroupManager> PlayableGroupManager;

	/**
	 * Keep track of the playables per channel.
	 * 
	 * The Playback object owns it's playables. However, the playables
	 * can share a playable group from other playback objects. This
	 * is necessary for the playback server that is always creating just
	 * one playback object per playable (atm).
	 */
	UPROPERTY(Transient)
	TMap<FName, FAvalanchePlaybackPlayableGroup> ChannelPlayableGroups;
	
	bool bIsPlaying = false;
	bool bIsDryRunningGraph = false;
	bool bAsyncDryRunRequested = false;
	bool bIsTicking = false;	// Tick reentrancy guard.
	FDelegateHandle TickDelegateHandle;

	/** Name of the preview channel this playback object is dedicated to. */
	FName PreviewChannelName;
	
	struct FPlaybackCommand
	{
		/** If the command has been pending for too long, it will be discarded. */
		FDateTime Timeout;
		
		FSoftObjectPath SourcePath;
		FString ChannelName;
		FName ChannelFName;

		bool HasTimedOut(const FDateTime& InNow) const
		{
			return InNow > Timeout;	
		}
	};
	struct FAnimationCommand : public FPlaybackCommand
	{
		EAvaMediaAnimAction AnimAction;
		FAnimPlaySettings AnimPlaySettings;
	};
	struct FRemoteControlCommand : public FPlaybackCommand
	{
		TSharedRef<FAvalancheRemoteControlValues> Values;
	};
	
	TArray<FAnimationCommand> PendingAnimationCommands;
	TArray<FRemoteControlCommand> PendingRemoteControlCommands;
};

/**
 * Somewhat reusable helper class to build a playback graph. 
 */
class FAvalanchePlaybackGraphBuilder
{
public:
	FAvalanchePlaybackGraphBuilder(UAvaMediaPlayableGroupManager* InPlayableGroupManager);
	~FAvalanchePlaybackGraphBuilder();

	/**
	 * @brief Connect the given node to the root node's pin corresponding to the given channel name.
	 * @param InChannelName Channel name to connect to.
	 * @param InNodeToConnect Node (already constructed) to connect.
	 */
	bool ConnectToRoot(const FString& InChannelName, UAvaPlaybackNode* InNodeToConnect);
	
	int32 GetPinIndexForChannel(const FString& InChannelName) const;

	UAvalanchePlayback* FinishBuilding();

	/** Helper function to construct a playback node. This will add the node in the playback's node list. */
	template<typename InPlaybackNodeType, typename = typename TEnableIf<TIsDerivedFrom<InPlaybackNodeType, UAvaPlaybackNode>::IsDerived>::Type>
		InPlaybackNodeType* ConstructPlaybackNode(TSubclassOf<InPlaybackNodeType> InNodeClass = nullptr, bool bInSelectNewNode = true)
	{
		if (Playback)
		{
			bFinished = false;
			return Playback->ConstructPlaybackNode<InPlaybackNodeType>(InNodeClass, bInSelectNewNode);
		}
		checkf(0, TEXT("Playback object is not created."));
		return nullptr;
	}
	
private:
	bool bFinished = false;
	UAvalanchePlayback* Playback = nullptr;
	UAvaPlaybackNodeRoot* RootNode = nullptr;
};