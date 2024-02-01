// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvalanchePlayback.h"

#include "Async/Async.h"
#include "AvaRenderTargetUtils.h"
#include "AvalancheBroadcast.h"
#include "AvalancheMediaSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/AvaGameInstance.h"
#include "IAvaMediaModule.h"
#include "IAvalancheBroadcastSettings.h"
#include "Misc/CoreDelegates.h"
#include "Playback/AvaMediaPlayableGroup.h"
#include "Playback/AvaMediaPlayableGroupManager.h"
#include "Playback/AvaMediaPlaybackManager.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/IAvaPlaybackEditor.h"
#include "Playback/Nodes/AvaPlaybackNode.h"
#include "Playback/Playables/AvalancheBlueprintPlayable.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#endif

DEFINE_LOG_CATEGORY(LogAvalanchePlayback);

UAvalanchePlayback::~UAvalanchePlayback()
{
	FAvaOutputChannel::GetOnChannelChanged().RemoveAll(this);
}

void UAvalanchePlayback::Play()
{
	if (!FAvaOutputChannel::GetOnChannelChanged().IsBoundToObject(this))
	{
		FAvaOutputChannel::GetOnChannelChanged().AddUObject(this, &UAvalanchePlayback::OnChannelChanged);
	}

	if (!IsPlaying())
	{
		SetIsPlaying(true);
	}	
}

void UAvalanchePlayback::LoadInstances()
{
	// Gather the asset references from the player nodes.
	ConditionalResolvePlaybackSettings();

	for (TPair<FName, FAvaPlaybackChannelParameters>& Playback : PlaybackSettings)
	{
		// Load the assets
		for (const FAvaSoftAssetPtr& AvalancheAsset : Playback.Value.AvalancheAssets)
		{
			FindOrLoadPlayable(AvalancheAsset, ResolveChannelName(Playback.Key));
		}
	}
}

// Unload playables.
void UAvalanchePlayback::UnloadInstances(EAvaPlaybackUnloadOptions InUnloadOptions)
{
	// We need to unload all the playables, not just the ones currently traversed by the graph.
	// We keep track of all the local playables in ChannelPlayableGroups. This now includes the
	// remote playables.
	
	const bool bIsForceImmediate = EnumHasAnyFlags(InUnloadOptions, EAvaPlaybackUnloadOptions::ForceImmediate);

	for (const TPair<FName, FAvalanchePlaybackPlayableGroup>& ChannelPlayableGroup : ChannelPlayableGroups)
	{
		TArray<UAvalanchePlayable*> Playables;
		ChannelPlayableGroup.Value.GetAllPlayables(Playables);
		
		for (UAvalanchePlayable* Playable : Playables)
		{
			FSoftObjectPath SourceAsset = Playable->GetSourceAssetPath();
			UnloadAndRemovePlayable(Playable, SourceAsset, ChannelPlayableGroup.Key, bIsForceImmediate);
		}
	}
}

void UAvalanchePlayback::Stop(EAvaPlaybackStopOptions InStopOptions)
{
	const bool bIsForceImmediate = EnumHasAnyFlags(InStopOptions, EAvaPlaybackStopOptions::ForceImmediate);
	const bool bIsUnload = EnumHasAnyFlags(InStopOptions, EAvaPlaybackStopOptions::Unload);

	// We need to unload all the playables, not just the ones currently traversed by the graph.
	// We keep track of all the local playables in ChannelPlayableGroups. This now includes the
	// remote playables.

	for (const TPair<FName, FAvalanchePlaybackPlayableGroup>& ChannelPlayableGroup : ChannelPlayableGroups)
	{
		TArray<UAvalanchePlayable*> Playables;
		ChannelPlayableGroup.Value.GetAllPlayables(Playables);
		
		for (UAvalanchePlayable* Playable : Playables)
		{
			// EndPlayWorld is no longer issued here as some of the engine systems don't seem
			// to support a EndPlay, BeginPlay sequence anymore. It is unknown if this situation
			// will change in the future, so we keep the option.
			Playable->EndPlay(EAvalanchePlayableEndPlayOptions::None);

			if (bIsUnload)
			{
				FSoftObjectPath SourceAsset = Playable->GetSourceAssetPath();
				UnloadAndRemovePlayable(Playable, SourceAsset, ChannelPlayableGroup.Key, bIsForceImmediate);
			}
		}
	}

	PlaybackSettings.Reset();
	PreviousPlaybackSettings.Reset();
	SetIsPlaying(false);
}

void UAvalanchePlayback::SetPlaybackManager(const TSharedPtr<FAvaMediaPlaybackManager>& InPlaybackManager)
{
	PlaybackManagerWeak = InPlaybackManager;

	if (IsManaged())
	{
		// Since the playback manager is going to manage ticking,
		// make sure to unregister the tick delegate.
		UnregisterTickDelegate();
	}
	else if (IsPlaying() && !TickDelegateHandle.IsValid())
	{
		RegisterTickDelegate();
	}
}

UAvalanchePlayable* UAvalanchePlayback::GetFirstPlayable() const
{
	for (const TPair<FName, FAvalanchePlaybackPlayableGroup>& ChannelPlayableGroup : ChannelPlayableGroups)
	{
		if (UAvalanchePlayable* Playable = ChannelPlayableGroup.Value.GetFirstPlayable())
		{
			return Playable;
		}
	}
	return nullptr;
}

void UAvalanchePlayback::GetAllPlayables(TArray<UAvalanchePlayable*>& OutPlayables) const
{
	for (const TPair<FName, FAvalanchePlaybackPlayableGroup>& ChannelPlayableGroup : ChannelPlayableGroups)
	{
		ChannelPlayableGroup.Value.GetAllPlayables(OutPlayables);
	}
}

void UAvalanchePlayback::ForEachPlayable(TFunctionRef<void(const UAvalanchePlayable*)> InFunction) const
{
	for (const TPair<FName, FAvalanchePlaybackPlayableGroup>& ChannelPlayableGroup : ChannelPlayableGroups)
	{
		ChannelPlayableGroup.Value.ForEachPlayable(InFunction);
	}
}

bool UAvalanchePlayback::HasPlayable(const UAvalanchePlayable* InPlayable) const
{
	for (const TPair<FName, FAvalanchePlaybackPlayableGroup>& ChannelPlayableGroup : ChannelPlayableGroups)
	{
		if (ChannelPlayableGroup.Value.HasPlayable(InPlayable))
		{
			return true;
		}
	}
	return false;
}

UAvalanchePlayable* UAvalanchePlayback::FindPlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName) const
{
	const FAvalanchePlaybackPlayableGroup* ChannelPlayableGroup = ChannelPlayableGroups.Find(InChannelName);
	return ChannelPlayableGroup ? ChannelPlayableGroup->FindPlayable(InSourceAssetPath) : nullptr;
}

UAvalanchePlayable* UAvalanchePlayback::FindOrLoadPlayable(const FAvaSoftAssetPtr& InSourceAsset, const FName& InChannelName)
{
	FAvalanchePlaybackPlayableGroup* ChannelPlayableGroup = ChannelPlayableGroups.Find(InChannelName);
	if (!ChannelPlayableGroup)
	{
		ChannelPlayableGroup = &ChannelPlayableGroups.Add(InChannelName, FAvalanchePlaybackPlayableGroup());
	}
	
	if (const TObjectPtr<UAvalanchePlayable>* FoundPlayable = ChannelPlayableGroup->Playables.Find(InSourceAsset.ToSoftObjectPath()))
	{
		return *FoundPlayable;
	}

	UAvalanchePlayable* NewPlayable = UAvalanchePlayable::Create(this, {GetPlayableGroupManager(), InSourceAsset, InChannelName});
	if (NewPlayable)
	{
		ChannelPlayableGroup->Playables.Add(InSourceAsset.ToSoftObjectPath(), NewPlayable);
		OnPlayableCreated.Broadcast(this, NewPlayable);
		NewPlayable->LoadAsset(InSourceAsset, false);
	}
	return NewPlayable;
}

void UAvalanchePlayback::RemovePlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName)
{
	if (FAvalanchePlaybackPlayableGroup* ChannelPlayableGroup = ChannelPlayableGroups.Find(InChannelName))
	{
		ChannelPlayableGroup->Playables.Remove(InSourceAssetPath);
	}
}

bool UAvalanchePlayback::UnloadAndRemovePlayable(UAvalanchePlayable* InPlayable, const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName, bool bInForceImmediate)
{
	bool bWorldUnloaded = false;
	if (InPlayable)
	{
		InPlayable->UnloadAsset(); // bIsForceImmediate is not used anymore.
		InPlayable->GetPlayableGroup()->UnregisterPlayable(InPlayable);
		// If no more playables in the group, we can request unload of the world too.
		bWorldUnloaded = InPlayable->GetPlayableGroup()->ConditionalRequestUnloadWorld(bInForceImmediate);
	}	
	RemovePlayable(InSourceAssetPath, InChannelName);
	return bWorldUnloaded;
}

bool UAvalanchePlayback::UnloadAndRemovePlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName, bool bInForceImmediate)
{
	return UnloadAndRemovePlayable(FindPlayable(InSourceAssetPath, InChannelName), InSourceAssetPath, InChannelName, bInForceImmediate);
}

TArray<UAvaGameInstance*> UAvalanchePlayback::GetActiveGameInstances() const
{
	TArray<UAvaGameInstance*> OutGameInstances;

	if (IsPlaying())
	{
		// TODO: revisit this. We have Active and Loaded.
		for (const TPair<FName, FAvaPlaybackChannelParameters>& Playback : PlaybackSettings)
		{
			FName ChannelName = ResolveChannelName(Playback.Key);
			for (const FAvaSoftAssetPtr& AvalancheAsset : Playback.Value.AvalancheAssets)
			{
				const UAvalanchePlayable* Playable = FindPlayable(AvalancheAsset.ToSoftObjectPath(), ChannelName);
				if (Playable && Playable->GetPlayableGroup())
				{
					if (UAvaGameInstance* AvaGameInstance = Cast<UAvaGameInstance>(Playable->GetPlayableGroup()->GetGameInstance()))
					{
						OutGameInstances.AddUnique(AvaGameInstance);
					}
				}
			}
		}
	}
	return OutGameInstances;
}

bool UAvalanchePlayback::HasPlayerNodeForSourceAvalancheAsset(const FSoftObjectPath& InSourceAssetPath) const
{
	for (const TObjectPtr<UAvaPlaybackNodePlayer>& PlayerNode : PlayerNodes)
	{
		if (PlayerNode && PlayerNode->GetAvalancheAssetPath() == InSourceAssetPath)
		{
			return true;
		}
	}
	return false;
}

void UAvalanchePlayback::SetPlayableGroupManager(UAvaMediaPlayableGroupManager* InPlayableGroupManager)
{
	PlayableGroupManager = InPlayableGroupManager;
}

UAvaMediaPlayableGroupManager* UAvalanchePlayback::GetGlobalPlayableGroupManager()
{
	return IAvaMediaModule::Get().GetLocalPlaybackManager().GetPlayableGroupManager();	
}

void UAvalanchePlayback::DryRunGraph(bool bDeferredExecution)
{
	if (bDeferredExecution)
	{
		//Only async dry run if we have not requested it yet, and it's not dry running already.
		if (!bAsyncDryRunRequested && !bIsDryRunningGraph)
		{
			bAsyncDryRunRequested = true;
			
			TWeakObjectPtr<UAvalanchePlayback> ThisWeak(this);
			AsyncTask(ENamedThreads::GameThread, [ThisWeak]()
			{
				if (ThisWeak.IsValid())
				{
					ThisWeak->DryRunGraphInternal();
				}
			});
		}
	}
	else
	{
		DryRunGraphInternal();
	}
}

TArray<FName> UAvalanchePlayback::GetChannelNamesForIndices(const TArray<int32>& InChannelIndices) const
{
	TArray<FName> OutChannelNames;
	if (IsPreviewOnly())
	{
		OutChannelNames.Reserve(1);
		OutChannelNames.Add(PreviewChannelName);
	}
	else if (InChannelIndices.Num() > 0)
	{
		const UAvalancheBroadcast& AvaBroadcast = UAvalancheBroadcast::Get();
		OutChannelNames.Reserve(InChannelIndices.Num());
		for (const int32 ChannelIndex : InChannelIndices)
		{
			const FName ChannelFName = AvaBroadcast.GetChannelName(ChannelIndex);
			if (!ChannelFName.IsNone())
			{
				OutChannelNames.Add(ChannelFName);
			}
		}
	}
	return OutChannelNames;
}

void UAvalanchePlayback::DryRunGraphInternal()
{
	//Everytime we run this, the Task has been complete.
	bAsyncDryRunRequested = false;
	
	if (!bIsDryRunningGraph)
	{
		TGuardValue<bool> Guard(bIsDryRunningGraph, true);
		if (RootNode)
		{
			TSet<UAvaPlaybackNode*> SeenNodes;
			TArray<UAvaPlaybackNode*> RemainingNodes;
			
			auto NotifyDryRun = [this, &SeenNodes, &RemainingNodes](void(UAvaPlaybackNode::*ExecFunc)())
				{
					//Reset but not deallocate the Memory that was used when previous NotifyDryRuns were called
					RemainingNodes.Reset();
					SeenNodes.Reset();
					
					RemainingNodes.Add(RootNode);
					
					while (RemainingNodes.Num() > 0)
					{
						UAvaPlaybackNode* const Node = RemainingNodes.Pop();
						if (Node && !SeenNodes.Contains(Node))
						{
							SeenNodes.Add(Node);
							(*Node.*ExecFunc)();
							RemainingNodes.Append(Node->GetChildNodes());
						}
					}
				};

			TArray<UAvaPlaybackNode*> Ancestors;
			
			NotifyDryRun(&UAvaPlaybackNode::PreDryRun);
			RootNode->DryRunNode(Ancestors);
			NotifyDryRun(&UAvaPlaybackNode::PostDryRun);
		}
	}
}

void UAvalanchePlayback::RegisterTickDelegate()
{
	if (ensure(!TickDelegateHandle.IsValid()))
	{
		TickDelegateHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &UAvalanchePlayback::OnEndFrameTick);
	}
}

void UAvalanchePlayback::UnregisterTickDelegate()
{
	if (TickDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}
}

void UAvalanchePlayback::OnEndFrameTick()
{
	Tick(FApp::GetDeltaTime());
}

void UAvalanchePlayback::ConditionalResolvePlaybackSettings()
{
	if (PlaybackSettings.IsEmpty() && IsValid(RootNode))
	{
		RootNode->TickRoot(0.016f, PlaybackSettings);
	}
}

void UAvalanchePlayback::SetRootNode(UAvaPlaybackNodeRoot* InRoot)
{
	if (IsValid(InRoot) && !IsValid(RootNode))
	{
		RootNode = InRoot;
	}
}

void UAvalanchePlayback::AddPlayerNode(UAvaPlaybackNodePlayer* InPlayer)
{
	if (IsValid(InPlayer))
	{
		PlayerNodes.AddUnique(InPlayer);
	}
}

UAvaPlaybackNodeRoot* UAvalanchePlayback::GetRootNode() const
{
	return RootNode;
}

void UAvalanchePlayback::AddPlaybackNode(UAvaPlaybackNode* Node)
{
	//Note: Ensure that all the Nodes added here have Outer set to this Playback Object.
	Node->Rename(nullptr, this);

#if WITH_EDITORONLY_DATA
	PlaybackNodes.Add(Node);
#endif
}

void UAvalanchePlayback::RemovePlaybackNode(UAvaPlaybackNode* Node)
{
#if WITH_EDITORONLY_DATA
	PlaybackNodes.Remove(Node);
#endif
}

void UAvalanchePlayback::StopPlayable(const FSoftObjectPath& InSourceAssetPath, const FName& InChannelName)
{
	if (UAvalanchePlayable* const Playable = FindPlayable(InSourceAssetPath, InChannelName))
	{
		Playable->EndPlay(EAvalanchePlayableEndPlayOptions::None);
	}
}

void UAvalanchePlayback::LoadAsset(const FAvaSoftAssetPtr& InSourceAsset, const FName& InChannelName)
{
	FindOrLoadPlayable(InSourceAsset, InChannelName);
}

void UAvalanchePlayback::Tick(float DeltaTime)
{
	if (bIsTicking)
	{
		return;
	}
	TGuardValue TickGuard(bIsTicking, true);

	if (!IsValid(RootNode))
	{
		return;
	}

	// Backup previous settings so we can detect if an asset has switched and needs to be stopped.
	PreviousPlaybackSettings = PlaybackSettings;
	
	//Pass 1: Tick from Root (Channels Node) up to the Player Nodes.
	for (TPair<FName, FAvaPlaybackChannelParameters>& Playback : PlaybackSettings)
	{
		Playback.Value.AvalancheAssets.Reset();
	}
	RootNode->TickRoot(DeltaTime, PlaybackSettings);

	//Post Pass 1: Refresh any Playback from the Settings gotten from pass 1
	for (const TPair<FName, FAvaPlaybackChannelParameters>& Playback : PlaybackSettings)
	{
		const FName ChannelName = ResolveChannelName(Playback.Key);
		FAvaOutputChannel& Channel = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannelMutable(ChannelName);

		for (const FAvaSoftAssetPtr& AvalancheAsset : Playback.Value.AvalancheAssets)
		{
			if (IsPreviewOnly())
			{
				RefreshPreview(AvalancheAsset, Channel, ChannelName);
			}
			else if (Channel.IsValidChannel())
			{
				RefreshPlayback(AvalancheAsset, Channel, ChannelName);
			}
		}

		// Check if the asset playing has changed (this may be the result of a switch/combiner node).
		if (const FAvaPlaybackChannelParameters* PreviousParameters = PreviousPlaybackSettings.Find(Playback.Key))
		{
			for (const FAvaSoftAssetPtr& PreviousAsset : PreviousParameters->AvalancheAssets)
			{
				if (Playback.Value.AvalancheAssets.Find(PreviousAsset) == INDEX_NONE)
				{
					StopPlayable(PreviousAsset.ToSoftObjectPath(), ChannelName);
				}
			}
		}
	}

	PreviousPlaybackSettings.Reset();

	//Pass 2: Tick Events from each Player Node to the Connected Event Triggers/Actions 
	for (TArray<TObjectPtr<UAvaPlaybackNodePlayer>>::TIterator Iter(PlayerNodes); Iter; ++Iter)
	{
		TObjectPtr<UAvaPlaybackNodePlayer> PlayerNode = *Iter;
		if (IsValid(PlayerNode))
		{
			PlayerNode->TickEventFeed(DeltaTime);
		}
		else
		{
			Iter.RemoveCurrent();
		}
	}

	//Post Pass 2: Reset the Event Triggers 
	for (UAvaPlaybackNodePlayer* PlayerNode : PlayerNodes)
	{
		if (IsValid(PlayerNode))
		{
			PlayerNode->ResetEvents();
		}
	}

	ExecutePendingAnimationCommands();
	ExecutePendingRemoteControlCommands();
}

void UAvalanchePlayback::PushAnimationCommand(const FSoftObjectPath& InSourceAssetPath, const FString& InChannelName, EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimSettings)
{
	const FDateTime Timeout = FDateTime::UtcNow() + FTimespan::FromSeconds(10);
	PendingAnimationCommands.Add({{Timeout, InSourceAssetPath, InChannelName, FName(InChannelName)}, InAnimAction, InAnimSettings});
}

void UAvalanchePlayback::PushRemoteControlValues(const FSoftObjectPath& InSourceAssetPath, const FString& InChannelName, const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues)
{
	const FDateTime Timeout = FDateTime::UtcNow() + FTimespan::FromSeconds(10);
	PendingRemoteControlCommands.Add({{Timeout, InSourceAssetPath, InChannelName, FName(InChannelName)}, InRemoteControlValues});
}

void UAvalanchePlayback::SetupPlaybackNode(UAvaPlaybackNode* InPlaybackNode, bool bSelectNewNode)
{
	check(InPlaybackNode);

	InPlaybackNode->CreateStartingConnectors();

#if WITH_EDITOR
	// Create the graph node
	check(InPlaybackNode->GetGraphNode() == nullptr);
	if (TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = GetGraphEditor())
	{
		GraphEditor->SetupPlaybackNode(EdGraph, InPlaybackNode, bSelectNewNode);
	}
#endif
}

#if WITH_EDITOR
const TArray<TObjectPtr<UAvaPlaybackNode>>& UAvalanchePlayback::GetPlaybackNodes() const
{
	return PlaybackNodes;
}

void UAvalanchePlayback::CreateGraph()
{
	TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = GetGraphEditor();
	if (EdGraph == nullptr && GraphEditor.IsValid())
	{
		EdGraph = GraphEditor->CreatePlaybackGraph(this);
        EdGraph->bAllowDeletion = false;
        
        // Give the schema a chance to fill out any required nodes (like the results node)
        const UEdGraphSchema* const Schema = EdGraph->GetSchema();
        Schema->CreateDefaultNodesForGraph(*EdGraph);
	}
}

void UAvalanchePlayback::ClearGraph()
{
	if (EdGraph)
	{
		EdGraph->Nodes.Empty();
		
		// Give the schema a chance to fill out any required nodes (like the results node)
		const UEdGraphSchema* const Schema = EdGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*EdGraph);
	}
}

void UAvalanchePlayback::RefreshPlaybackNode(UAvaPlaybackNode* InPlaybackNode)
{
	if (TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = GetGraphEditor())
	{
		if (InPlaybackNode && InPlaybackNode->GetGraphNode())
		{
			GraphEditor->RefreshNode(*InPlaybackNode->GetGraphNode());
		}
	}
}

void UAvalanchePlayback::CompilePlaybackNodesFromGraphNodes()
{
	if (TSharedPtr<IAvaPlaybackGraphEditor> GraphEditor = GetGraphEditor())
	{
		GraphEditor->CompilePlaybackNodesFromGraphNodes(this);
	}
}

UEdGraph* UAvalanchePlayback::GetGraph()
{
	return EdGraph;
}

void UAvalanchePlayback::ResetGraph()
{
	for (const UAvaPlaybackNode* const PlaybackNode : PlaybackNodes)
	{
		EdGraph->RemoveNode(PlaybackNode->GetGraphNode());
	}

	PlaybackNodes.Reset();
}

void UAvalanchePlayback::SetGraphEditor(TSharedPtr<IAvaPlaybackGraphEditor> InGraphEditor)
{
	GraphEditorWeak = InGraphEditor;
}

TSharedPtr<IAvaPlaybackGraphEditor> UAvalanchePlayback::GetGraphEditor() const
{
	return GraphEditorWeak.Pin();
}
#endif

void UAvalanchePlayback::ExecutePendingAnimationCommands()
{
	if (PendingAnimationCommands.IsEmpty())
	{
		return;
	}

	const UAvalancheBroadcast& AvaBroadcast = UAvalancheBroadcast::Get();

	// If commands can't be executed because the assets are still
	// streaming, we will accumulate them in this array for next time.
	// Note: preserves the order of the commands (ie. not a map or set).
	TArray<FAnimationCommand> StillPendingAnimationCommands;

	const FDateTime CurrentTime = FDateTime::UtcNow();

	for (FAnimationCommand& AnimCommand : PendingAnimationCommands)
	{
		if (AnimCommand.HasTimedOut(CurrentTime))
		{
			UE_LOG(LogAvalanchePlayback, Warning,
				TEXT("Animation command for asset \"%s\" on channel \"%s\" timed out and is discarded."),
				*AnimCommand.SourcePath.ToString(), *AnimCommand.ChannelName);
			continue;
		}

		FAnimPlaySettings& AnimSettings = AnimCommand.AnimPlaySettings;
		AnimSettings.Action = AnimCommand.AnimAction; // TODO: get rid of the AnimAction in 2 places.
		
		UAvalanchePlayable* Playable = FindPlayable(AnimCommand.SourcePath, AnimCommand.ChannelFName);
		if (!Playable)
		{
			continue;
		}
		
		const EAvalanchePlayableCommandResult Result = Playable->ExecuteAnimationCommand(AnimCommand.AnimAction, AnimCommand.AnimPlaySettings);
		if (Result == EAvalanchePlayableCommandResult::KeepPending)
		{
			StillPendingAnimationCommands.Add(AnimCommand);
		}
	}
	PendingAnimationCommands = StillPendingAnimationCommands;
}

void UAvalanchePlayback::ExecutePendingRemoteControlCommands()
{
	if (PendingRemoteControlCommands.IsEmpty())
	{
		return;
	}

	// If commands can't be executed because the assets are still
	// streaming, we will accumulate them in this array for next time.
	// Note: preserves the order of the commands (ie. not a map or set).
	TArray<FRemoteControlCommand> StillPendingRemoteControlCommands;

	const FDateTime CurrentTime = FDateTime::UtcNow();
	
	for (const FRemoteControlCommand& RemoteControlCommand : PendingRemoteControlCommands)
	{
		if (RemoteControlCommand.HasTimedOut(CurrentTime))
		{
			UE_LOG(LogAvalanchePlayback, Warning,
				TEXT("Remote Control command for asset \"%s\" on channel \"%s\" timed out and is discarded."),
				*RemoteControlCommand.SourcePath.ToString(), *RemoteControlCommand.ChannelName);
			continue;
		}
		
		UAvalanchePlayable* Playable = FindPlayable(RemoteControlCommand.SourcePath, RemoteControlCommand.ChannelFName);
		if (!Playable)
		{
			continue;
		}

		const EAvalanchePlayableCommandResult Result = Playable->UpdateRemoteControlCommand(RemoteControlCommand.Values);
		if (Result == EAvalanchePlayableCommandResult::KeepPending)
		{
			StillPendingRemoteControlCommands.Add(RemoteControlCommand);
		}
	}
	PendingRemoteControlCommands = StillPendingRemoteControlCommands;
}

void UAvalanchePlayback::OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaChannelChange::State))
	{
		OnChannelBroadcastStateChanged(InChannel);
	}
	
	if (EnumHasAnyFlags(InChange, EAvaChannelChange::Settings))
	{
		// Need to propagate the channel settings to loaded/playing game instances.
		if (const FAvalanchePlaybackPlayableGroup* ChannelPlayableGroup = ChannelPlayableGroups.Find(InChannel.GetChannelName()))
		{
			TSet<const UAvaMediaPlayableGroup*> PlayableGroups; 
			ChannelPlayableGroup->ForEachPlayable([&PlayableGroups](const UAvalanchePlayable* InPlayable)
			{
				if (const UAvaMediaPlayableGroup* PlayableGroup = InPlayable->GetPlayableGroup())
				{
					PlayableGroups.Add(PlayableGroup);
				}
			});
			for (const UAvaMediaPlayableGroup* PlayableGroup : PlayableGroups)
			{
				if (const UAvaGameInstance* GameInstance = Cast<UAvaGameInstance>(PlayableGroup->GetGameInstance()))
				{
					if (UAvaGameViewportClient* ViewportClient = GameInstance->GetAvaGameViewportClient())
					{
						FAvaViewportQualitySettings QualitySettingsMutable = InChannel.GetViewportQualitySettings(); 
						QualitySettingsMutable.Apply(ViewportClient->EngineShowFlags);
					}
				}
			}
		}
	}
}

void UAvalanchePlayback::OnChannelBroadcastStateChanged(const FAvaOutputChannel& InChannel)
{
	if (IsPlaying() && InChannel.IsValidChannel() && InChannel.GetState() == EAvaChannelState::Live)
	{
		const FName ChangedChannelName = InChannel.GetChannelName();
		FAvaBroadcastProfile& Profile = UAvalancheBroadcast::Get().GetCurrentProfile();
		for (const TPair<FName, FAvaPlaybackChannelParameters>& Playback : PlaybackSettings)
		{
			const FName ChannelName = ResolveChannelName(Playback.Key);
			
			// Ensure to route the preview into it's channel regardless of pin connections.
			// The preview channel(s) don't have corresponding pins.
			if (ChannelName == ChangedChannelName)
			{
				FAvaOutputChannel& Channel = Profile.GetChannelMutable(ChannelName);
				for (const FAvaSoftAssetPtr& AvalancheAsset : Playback.Value.AvalancheAssets)
				{
					RefreshPlayback(AvalancheAsset, Channel, ChannelName);
				}
			}
		}
	}
}

void UAvalanchePlayback::SetIsPlaying(bool bInIsPlaying)
{
	if (bIsPlaying != bInIsPlaying)
	{
		bIsPlaying = bInIsPlaying;
		if (bInIsPlaying && !IsManaged())
		{
			RegisterTickDelegate();
		}
#if WITH_EDITOR
		for (const TObjectPtr<UAvaPlaybackNode>& Node : PlaybackNodes)
		{
			if (Node)
			{
				Node->NotifyPlaybackStateChanged(bInIsPlaying);
			}
		}
#else
		RootNode->NotifyPlaybackStateChanged(bInIsPlaying);
#endif
		
		OnPlaybackStateChanged.Broadcast(bInIsPlaying);
	}

	if (!bInIsPlaying)
	{
		UnregisterTickDelegate();
	}
}

// Remark: very similar to FAvaOutputChannel::IsChannelAvailableForPlayback, could we possibly merge the logic?
bool UAvalanchePlayback::CanRefreshPlayback(const FAvaSoftAssetPtr& InSourceAsset, const FAvaOutputChannel& InChannel) const
{
	return IsPlaying() && InChannel.IsValidChannel() && !InSourceAsset.IsNull();
}

bool UAvalanchePlayback::RefreshPlayback(const FAvaSoftAssetPtr& InSourceAsset, FAvaOutputChannel& InChannel, const FName& InChannelName)
{
	if (!CanRefreshPlayback(InSourceAsset, InChannel))
	{
		InChannel.UpdateRenderTarget(nullptr, nullptr);
		InChannel.UpdateAudioDevice(FAudioDeviceHandle());
		return false;
	}
	
	if (UAvalanchePlayable* const Playable = FindOrLoadPlayable(InSourceAsset, InChannelName))
	{
		UTextureRenderTarget2D* const RenderTarget = UpdatePlaybackRenderTarget(Playable, InChannel);

		const FIntPoint ViewportSize = RenderTarget
			? AvaRenderTargetUtils::GetRenderTargetSize(RenderTarget)
			: InChannel.DetermineRenderTargetSize();

		const FAvaInstancePlaySettings WorldPlaySettings =
		{ IAvaMediaModule::Get().GetAvaInstanceSettings(), InChannelName, RenderTarget, ViewportSize, InChannel.GetViewportQualitySettings() };

		Playable->BeginPlay(WorldPlaySettings);

		InChannel.UpdateAudioDevice(Playable->GetPlayWorld() ? Playable->GetPlayWorld()->GetAudioDevice() : FAudioDeviceHandle());
		InChannel.UpdateRenderTarget(Playable->GetPlayableGroup(), RenderTarget);

		return Playable->IsPlaying();
	}

	// Fallback
	InChannel.UpdateRenderTarget(nullptr, nullptr);
	InChannel.UpdateAudioDevice(FAudioDeviceHandle());
	return false;
}

bool UAvalanchePlayback::CanRefreshPreview(const FAvaSoftAssetPtr& InSourceAsset) const
{
	return IsPlaying() && !InSourceAsset.IsNull();
}

bool UAvalanchePlayback::RefreshPreview(const FAvaSoftAssetPtr& InSourceAsset, FAvaOutputChannel& InChannel, const FName& InChannelName)
{
	// If the preview channel has been setup, go through the normal channel refresh.
	if (InChannel.IsValidChannel())
	{
		return RefreshPlayback(InSourceAsset, InChannel, InChannelName);
	}

	if (!CanRefreshPreview(InSourceAsset))
	{
		return false;
	}

	UAvalanchePlayable* const Playable = FindOrLoadPlayable(InSourceAsset, InChannelName);

	if (!Playable)
	{
		return false;
	}
	
	UTextureRenderTarget2D* RenderTarget = Playable->GetPlayableGroup()->RenderTarget;

	if (!RenderTarget)
	{
		// The render target is created here for now.
		// TODO: adjust to preview window's size.
		static const FName PreviewRenderTargetBaseName = TEXT("AvaPlayback_PreviewRenderTarget");
		RenderTarget = AvaRenderTargetUtils::CreateDefaultRenderTarget(PreviewRenderTargetBaseName);
		AvaRenderTargetUtils::UpdateRenderTarget(RenderTarget,
			UAvalancheMediaSettings::Get().PreviewDefaultResolution,
			FAvaOutputChannel::GetDefaultMediaOutputFormat(),
			IAvaMediaModule::Get().GetBroadcastSettings().GetChannelClearColor());

		Playable->GetPlayableGroup()->RenderTarget = RenderTarget;
	}
	
	const FIntPoint ViewportSize = AvaRenderTargetUtils::GetRenderTargetSize(RenderTarget);

	static const FAvaViewportQualitySettings DefaultPreviewQualitySettings;
	const FAvaInstancePlaySettings WorldPlaySettings =
		{ IAvaMediaModule::Get().GetAvaInstanceSettings(), InChannelName, RenderTarget, ViewportSize, DefaultPreviewQualitySettings };

	Playable->BeginPlay(WorldPlaySettings);
	
	return Playable->IsPlaying();
}

UTextureRenderTarget2D* UAvalanchePlayback::UpdatePlaybackRenderTarget(const UAvalanchePlayable* InPlayable
	, const FAvaOutputChannel& InChannel)
{
	UTextureRenderTarget2D* RenderTarget = nullptr;
	
	if (InChannel.IsValidChannel())
	{
		// Preferably use the channel's placeholder RT to avoid having the MediaCapture
		// switch and possibly cause rendering glitches.
		RenderTarget = InChannel.GetPlaceholderRenderTarget();

		// Only fallback to internal RTs if we can't get one from the channels.
		if (!IsValid(RenderTarget))
		{
			const FName ChannelName = InChannel.GetChannelName();

			// See if we have a managed render target for this asset in this channel.
			RenderTarget = InPlayable->GetPlayableGroup()->RenderTarget;
			
			if (!IsValid(RenderTarget))
			{
				static const FName PlaybackRenderTargetBaseName = TEXT("AvaPlayback_RenderTarget");
				RenderTarget = AvaRenderTargetUtils::CreateDefaultRenderTarget(PlaybackRenderTargetBaseName);

				// This render target is now managed by the playable's instance group.
				InPlayable->GetPlayableGroup()->RenderTarget = RenderTarget;
			}
		}
		
		check(RenderTarget);

		AvaRenderTargetUtils::UpdateRenderTarget(RenderTarget,
			InChannel.DetermineRenderTargetSize(),
			InChannel.DetermineRenderTargetFormat(),
			IAvaMediaModule::Get().GetBroadcastSettings().GetChannelClearColor());
	}
	
	return RenderTarget;
}

FAvalanchePlaybackGraphBuilder::FAvalanchePlaybackGraphBuilder(UAvaMediaPlayableGroupManager* InPlayableGroupManager)
{
	Playback = NewObject<UAvalanchePlayback>();
	Playback->SetPlayableGroupManager(InPlayableGroupManager);
	
	// Construct Root
	RootNode = Playback->ConstructPlaybackNode<UAvaPlaybackNodeRoot>();
}

FAvalanchePlaybackGraphBuilder::~FAvalanchePlaybackGraphBuilder()
{
	FinishBuilding();
}

bool FAvalanchePlaybackGraphBuilder::ConnectToRoot(const FString& InChannelName, UAvaPlaybackNode* InNodeToConnect)
{
	if (!InNodeToConnect)
	{
		return false;
	}
	
	// Connect Player Node to Root Node
	TArray<UAvaPlaybackNode*> NewRootChildNodes;
	NewRootChildNodes.AddZeroed(RootNode->GetMaxChildNodes());

	// Copy existing children.
	const TArray<TObjectPtr<UAvaPlaybackNode>>& ExistingChildren = RootNode->GetChildNodes();
	for (int32 PinIndex = 0; PinIndex < ExistingChildren.Num(); ++PinIndex)
	{
		if (NewRootChildNodes.IsValidIndex(PinIndex))
		{
			NewRootChildNodes[PinIndex] = ExistingChildren[PinIndex];
		}
	}

	// Get the pin index corresponding to the given channel name.
	const int32 ConnectionPinIndex = GetPinIndexForChannel(InChannelName);

	// Check that Index is valid. RootNode Max Child Nodes should be the Channel Names Count,
	// and the Connection Pin Index should be >= 0.
	if (NewRootChildNodes.IsValidIndex(ConnectionPinIndex))
	{
		NewRootChildNodes[ConnectionPinIndex] = InNodeToConnect;
		RootNode->SetChildNodes(MoveTemp(NewRootChildNodes));
		bFinished = false;
		return true;
	}
	else
	{
		UE_LOG(LogAvalanchePlayback, Error
			, TEXT("Using index %d for channel \"%s\" is invalid. Root node has %d children. The node \"%s\" could not be added.")
			, ConnectionPinIndex, *InChannelName, NewRootChildNodes.Num(), *InNodeToConnect->GetNodeDisplayNameText().ToString());
	}
	return false;	
}

int32 FAvalanchePlaybackGraphBuilder::GetPinIndexForChannel(const FString& InChannelName) const
{
	int32 ConnectionPinIndex = UAvalancheBroadcast::Get().GetChannelIndex(*InChannelName);

	// We could return nullptr here, but given that this is a "GetOrCreate",
	// the most sensible thing is to just connect to first index and continue
	if (ConnectionPinIndex == INDEX_NONE)
	{
		UE_LOG(LogAvalanchePlayback, Warning
			, TEXT("No channel with name (%s) was found. Connecting player node to first channel available")
			, *InChannelName);
	
		ConnectionPinIndex = 0;
	}
	return ConnectionPinIndex;
}

UAvalanchePlayback* FAvalanchePlaybackGraphBuilder::FinishBuilding()
{
	if (Playback && !bFinished)
	{
		Playback->DryRunGraph();
		bFinished = true;
	}
	return Playback;
}
