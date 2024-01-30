// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/AvalanchePlayable.h"

#include "AvaSequence.h"
#include "AvaSequencePlayer.h"
#include "AvalancheBroadcast.h"
#include "Engine/Engine.h"
#include "Framework/AvaSoftAssetPtr.h"
#include "IAvaSceneInterface.h"
#include "IAvaSequenceProvider.h"
#include "Playback/AvaMediaPlayableGroup.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playback/AvalancheRemoteControlValues.h"
#include "Playback/Playables/AvalancheBlueprintPlayable.h"
#include "Playback/Playables/AvalancheLevelStreamingPlayable.h"
#include "Playback/Playables/AvalancheRemoteProxyPlayable.h"
#include "RemoteControlPreset.h"

DEFINE_LOG_CATEGORY(LogAvalanchePlayable);

#define LOCTEXT_NAMESPACE "AvalanchePlayable"

namespace UE::AvalanchePlayable::Private
{
	bool ShouldCreateLocalPlayable(const FName& InChannelName, const UAvalancheBroadcast& InBroadcast)
	{
		const FAvaOutputChannel& Channel = InBroadcast.GetCurrentProfile().GetChannel(InChannelName);

		// If there is no broadcast channel defined, like for preview (by default), then this is a local playable.
		if (!Channel.IsValidChannel())
		{
			return true;
		}

		// For non-preview, the commands will be executed locally if the channel has at least one local outputs or no outputs.
		// The "no outputs" condition is considered valid. Empty channels run locally.
		if (Channel.HasAnyLocalMediaOutputs() || Channel.GetMediaOutputs().IsEmpty())
		{
			return true;
		}
		
		return false;
	}

	bool HasRemoteOutputs(const FName& InChannelName, const UAvalancheBroadcast& InBroadcast)
	{
		const FAvaOutputChannel& Channel = InBroadcast.GetCurrentProfile().GetChannel(InChannelName);
		return Channel.IsValidChannel() && Channel.HasAnyRemoteMediaOutputs();
	}

	FString GetPrettyPlayableInfo(const UAvalanchePlayable* InPlayable)
	{
		if (InPlayable)
		{
			return FString::Printf(TEXT("Id:%s, Asset:%s, Status:%s"),
				*InPlayable->GetInstanceId().ToString(),
				*InPlayable->GetSourceAssetPath().ToString(),
				*StaticEnum<EAvalanchePlayableStatus>()->GetNameByValue(static_cast<int32>(InPlayable->GetPlayableStatus())).ToString());
		}
		return TEXT("(nullptr)");
	}

	FString GetBriefFrameInfo()
	{
		return UE::AvaMediaPlayback::Utils::GetBriefFrameInfo();
	}
	
	FString GetPrettySequenceCommandInfo(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings)
	{
		return FString::Printf(TEXT("Action:%s, Name:%s"),
			*StaticEnum<EAvaMediaAnimAction>()->GetNameByValue(static_cast<int32>(InAnimAction)).ToString(),
			*InAnimPlaySettings.AnimationName.ToString());
	}
}

UAvalanchePlayable::FOnSequenceEvent UAvalanchePlayable::OnSequenceEventDelegate;
UAvalanchePlayable::FOnTransitionEvent UAvalanchePlayable::OnTransitionEventDelegate;

UAvalanchePlayable* UAvalanchePlayable::Create(UObject* InOuter, const FPlayableCreationInfo& InPlayableInfo)
{
	using namespace UE::AvalanchePlayable::Private;

	const UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
	
	UAvalanchePlayable* NewPlayable;
	
	// Forked channels considerations:
	// - The case of forked remote channels is/will be handled internally to the RemoteProxy playable.
	// - The case of forked local and remote channels will lead to a local playable and a remote proxy playable.
	//   It would require wrapping the playables in a composite (or facade?) proxy. TODO
	
	if (ShouldCreateLocalPlayable(InPlayableInfo.ChannelName, Broadcast))
	{
		// For the moment, remote outputs will be ignored.
		if (HasRemoteOutputs(InPlayableInfo.ChannelName, Broadcast))
		{
			UE_LOG(LogAvalanchePlayable, Error, TEXT("Forked Channels with both local and remote outputs are not supported in this version. Only local instance will be created."));
		}
		
		NewPlayable = CreateLocalPlayable(InOuter, InPlayableInfo);
	}
	else
	{
		// Purely remote channel.
		NewPlayable = CreateRemoteProxyPlayable(InOuter, InPlayableInfo);
	}

	// Finish the setup.
	if (NewPlayable && !NewPlayable->InitPlayable(InPlayableInfo))
	{
		// final setup may fail, in this case the playable is discarded.
		return nullptr;
	}

	return NewPlayable;
}

const FSoftObjectPath& UAvalanchePlayable::GetSourceAssetPath() const
{
	static const FSoftObjectPath Empty;
	return Empty;
}

EAvalanchePlayableCommandResult UAvalanchePlayable::ExecuteAnimationCommand(EAvaMediaAnimAction InAnimAction, const FAnimPlaySettings& InAnimPlaySettings)
{
	using namespace UE::AvalanchePlayable::Private;

	const EAvalanchePlayableStatus PlayableStatus = GetPlayableStatus();

	if (PlayableStatus == EAvalanchePlayableStatus::Unknown
		|| PlayableStatus == EAvalanchePlayableStatus::Error
		|| PlayableStatus == EAvalanchePlayableStatus::Unloaded)
	{
		UE_LOG(LogAvalanchePlayable, Verbose,
			TEXT("%s Playable {%s} -> Discarding Sequence Command: {%s}."),
			*GetBriefFrameInfo(), *GetPrettyPlayableInfo(this), *GetPrettySequenceCommandInfo(InAnimAction, InAnimPlaySettings));
	
		// Discard the command
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}

	// Asset status must be visible to run the animation commands.
	// If not visible, the components are not yet added to the world and animations won't execute.
	if (PlayableStatus != EAvalanchePlayableStatus::Visible)
	{
		UE_LOG(LogAvalanchePlayable, Verbose,
			TEXT("%s Playable {%s} -> ReQueueing Sequence Command: {%s}."),
			*GetBriefFrameInfo(), *GetPrettyPlayableInfo(this), *GetPrettySequenceCommandInfo(InAnimAction, InAnimPlaySettings));
		
		// Keep the command in the queue for next tick.
		return EAvalanchePlayableCommandResult::KeepPending;
	}

	const IAvaSceneInterface* AvalancheScene = GetSceneInterface();
	if (!AvalancheScene)
	{
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}
	
	IAvaSequencePlaybackObject* PlaybackObject = AvalancheScene->GetPlaybackObject();
	
	if (!PlaybackObject)
	{
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}

	const IAvaSequenceProvider* SequenceProvider = AvalancheScene->GetSequenceProvider();

	if (!SequenceProvider)
	{
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}

	UE_LOG(LogAvalanchePlayable, Verbose,
		TEXT("%s Playable {%s} -> Executing Sequence Command: {%s}."),
		*GetBriefFrameInfo(), *GetPrettyPlayableInfo(this), *GetPrettySequenceCommandInfo(InAnimAction, InAnimPlaySettings));
	
	for (const TObjectPtr<UAvaSequence>& Sequence : SequenceProvider->GetSequences())
	{
		// Remark: if the command doesn't specify the sequence name, we run the command on all the sequences.
		if (Sequence && (Sequence->GetFName() == InAnimPlaySettings.AnimationName || InAnimPlaySettings.AnimationName.IsNone()))
		{
			if (InAnimAction == EAvaMediaAnimAction::Play || InAnimAction == EAvaMediaAnimAction::PreviewFrame)
			{
				UAvaSequencePlayer* const SequencePlayer = PlaybackObject->PlaySequence(Sequence, InAnimPlaySettings.AsPlayParams());
				if (InAnimAction == EAvaMediaAnimAction::PreviewFrame && SequencePlayer)
				{
					SequencePlayer->PreviewFrame();
				}
			}
			else if (InAnimAction == EAvaMediaAnimAction::Continue)
			{
				PlaybackObject->ContinueSequence(Sequence);
			}
			else if (InAnimAction == EAvaMediaAnimAction::Stop)
			{
				PlaybackObject->StopSequence(Sequence);
			}
		}
	}
	
	return EAvalanchePlayableCommandResult::Executed;
}

EAvalanchePlayableCommandResult UAvalanchePlayable::UpdateRemoteControlCommand(const TSharedRef<FAvalancheRemoteControlValues>& InRemoteControlValues)
{
	const EAvalanchePlayableStatus PlayableStatus = GetPlayableStatus();

	if (PlayableStatus == EAvalanchePlayableStatus::Unknown
		|| PlayableStatus == EAvalanchePlayableStatus::Error
		|| PlayableStatus == EAvalanchePlayableStatus::Unloaded)
	{
		using namespace UE::AvalanchePlayable::Private;
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable {%s} -> Discarding RC Update."), *GetBriefFrameInfo(), *GetPrettyPlayableInfo(this));

		// Discard the command
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}

	// Asset status must be visible to run the command.
	// If not visible, the components are not yet added to the world.
	if (PlayableStatus != EAvalanchePlayableStatus::Visible)
	{
		using namespace UE::AvalanchePlayable::Private;
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable {%s} -> ReQueueing RC Update."), *GetBriefFrameInfo(), *GetPrettyPlayableInfo(this));

		// Keep the command in the queue for next tick.
		return EAvalanchePlayableCommandResult::KeepPending;
	}

	const IAvaSceneInterface* AvalancheScene = GetSceneInterface();
	if (!AvalancheScene)
	{
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}
	
	URemoteControlPreset* RemoteControlPreset = AvalancheScene->GetRemoteControlPreset();
	
	if (!IsValid(RemoteControlPreset))
	{
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Remote Control command for asset \"%s\": Remote Control Preset is null."),
			*GetSourceAssetPath().ToString());
		return EAvalanchePlayableCommandResult::ErrorDiscard;
	}

	LatestRemoteControlValues = InRemoteControlValues;

	using namespace UE::AvalanchePlayable::Private;
	UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable {%s} -> Executing RC Update."), *GetBriefFrameInfo(), *GetPrettyPlayableInfo(this));

	// WYSIWYG (Solution): For the runtime/playback RCP, we don't apply the controllers.
	// We assume the controller actions are already executed in the playlist's managed RCP
	// during page edition and the resulting entity values are already captured.
	InRemoteControlValues->ApplyEntityValuesToRemoteControlPreset(RemoteControlPreset);

	return EAvalanchePlayableCommandResult::Executed;
}

void UAvalanchePlayable::BeginPlay(const FAvalancheInstancePlaySettings& InWorldPlaySettings)
{
	if (!PlayableGroup)
	{
		return;
	}

	const bool bGroupHasBegunPlay = PlayableGroup->ConditionalBeginPlay(InWorldPlaySettings);
	
	if (!bIsPlaying || bGroupHasBegunPlay)
	{
		bIsPlaying = true;
		
		// Playable events need to transit through playback events to reach the playlist for proper impl layer separation.
		UAvaSequencePlayer::OnSequenceStarted().AddUObject(this, &UAvalanchePlayable::HandleOnSequenceStarted);
		UAvaSequencePlayer::OnSequenceFinished().AddUObject(this, &UAvalanchePlayable::HandleOnSequenceFinished);

		OnPlay();
	}
}

void UAvalanchePlayable::EndPlay(EAvalanchePlayableEndPlayOptions InOptions)
{
	if (!bIsPlaying)
	{
		return;
	}

	bIsPlaying = false;
	UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	OnEndPlay();

	if (PlayableGroup)
	{
		PlayableGroup->UpdateCameraSetup();

		if (EnumHasAnyFlags(InOptions, EAvalanchePlayableEndPlayOptions::ConditionalEndPlayWorld) && !PlayableGroup->HasPlayingPlayables())
		{
			const bool bForceImmediate = EnumHasAnyFlags(InOptions, EAvalanchePlayableEndPlayOptions::ForceImmediate);
			PlayableGroup->RequestEndPlayWorld(bForceImmediate);
		}
	}
}

bool UAvalanchePlayable::HasSequence(const UAvaSequence* InSequence) const
{
	const IAvaSceneInterface* SceneInterface = GetSceneInterface();
	if (!SceneInterface)
	{
		return false;
	}
	
	const IAvaSequenceProvider* SequenceProvider = SceneInterface->GetSequenceProvider();
	if (!SequenceProvider)
	{
		return false;
	}
	
	for (const TObjectPtr<UAvaSequence>& Sequence : SequenceProvider->GetSequences())
	{
		if (Sequence == InSequence)
		{
			return true;
		}
	}
	return false;
}

bool UAvalanchePlayable::InitPlayable(const FPlayableCreationInfo& InPlayableInfo)
{
	if (PlayableGroup)
	{
		// Register this playable in the instance group.
		// This is necessary to determine what is playing in what group.
		PlayableGroup->RegisterPlayable(this);
		return true;
	}
	
	// Currently, playables must have a playable group otherwise they are unplayable.
	UE_LOG(LogAvalanchePlayable, Error, TEXT("Failed to create or acquire a playable group for \"%s\". Playable will be discarded."), *InPlayableInfo.SourceAsset.ToSoftObjectPath().ToString());
	return false;
}

void UAvalanchePlayable::HandleOnSequenceStarted(UAvaSequencePlayer* InSequencePlayer, UAvaSequence* InSequence)
{
	if (HasSequence(InSequence))
	{
		using namespace UE::AvalanchePlayable::Private;
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable {%s}: Sequence \"%s\" started."),
			*GetBriefFrameInfo(), *GetPrettyPlayableInfo(this), *InSequence->GetFName().ToString());
		OnSequenceEventDelegate.Broadcast(this, InSequence->GetFName(), EAvalanchePlayableSequenceEventType::Started);
	}
}

void UAvalanchePlayable::HandleOnSequenceFinished(UAvaSequencePlayer* InSequencePlayer, UAvaSequence* InSequence)
{
	if (HasSequence(InSequence))
	{
		using namespace UE::AvalanchePlayable::Private;
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable {%s}: Sequence \"%s\" finished."),
			*GetBriefFrameInfo(), *GetPrettyPlayableInfo(this), *InSequence->GetFName().ToString());
		OnSequenceEventDelegate.Broadcast(this, InSequence->GetFName(), EAvalanchePlayableSequenceEventType::Finished);
	}
}

UAvalanchePlayable* UAvalanchePlayable::CreateLocalPlayable(UObject* InOuter, const FPlayableCreationInfo& InPlayableInfo)
{
	switch (InPlayableInfo.SourceAsset.GetAssetType())
	{
	case EAvalancheAssetType::Blueprint:
		return NewObject<UAvalancheBlueprintPlayable>(InOuter ? InOuter : GEngine);
	case EAvalancheAssetType::World:
		return NewObject<UAvalancheLevelStreamingPlayable>(InOuter ? InOuter : GEngine);
	default:
		UE_LOG(LogAvalanchePlayable, Error, TEXT("Asset \"%s\" is an unsupported type."), *InPlayableInfo.SourceAsset.ToSoftObjectPath().ToString());
		return nullptr;
	}
}

UAvalanchePlayable* UAvalanchePlayable::CreateRemoteProxyPlayable(UObject* InOuter, const FPlayableCreationInfo& InPlayableInfo)
{
	return NewObject<UAvalancheRemoteProxyPlayable>(InOuter ? InOuter : GEngine);
}

#undef LOCTEXT_NAMESPACE
