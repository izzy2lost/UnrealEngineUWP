// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Transition/AvaLocalPlayableTransition.h"

#include "AvaSequencePlayer.h"
#include "AvaTransitionSubsystem.h"
#include "AvaTransitionTree.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "Execution/AvaTransitionExecutorBuilder.h"
#include "Execution/IAvaTransitionExecutor.h"
#include "IAvaSceneInterface.h"
#include "IAvaSequenceProvider.h"
#include "Playback/AvaMediaPlaybackUtils.h"
#include "Playback/AvalanchePlayable.h"
#include "Playback/Transition/AvaPlayableTransitionPrivate.h"
#include "Playback/Transition/AvaTransitionPlayableScene.h"

namespace UE::AvaPlayableTransition::Private
{
	UAvaTransitionSubsystem* GetTransitionSubsystem(const UAvalanchePlayable& InPlayable)
	{
		if (const UAvaMediaPlayableGroup* PlayableGroup = InPlayable.GetPlayableGroup())
		{
			if (const UWorld* World = PlayableGroup->GetPlayWorld())
			{
				return World->GetSubsystem<UAvaTransitionSubsystem>();
			}
		}
		return nullptr;
	}

	IAvaTransitionBehavior* GetTransitionBehavior(const UAvalanchePlayable& InPlayable, const UAvaTransitionSubsystem& InTransitionSubsystem)
	{
		if (const IAvaSceneInterface* SceneInterface = InPlayable.GetSceneInterface())
		{
			if (ULevel* const Level = SceneInterface->GetSceneLevel())
			{
				return InTransitionSubsystem.GetTransitionBehavior(Level);
			}
		}
		return nullptr;		
	}

	IAvaTransitionBehavior* GetTransitionBehavior(const UAvalanchePlayable& InPlayable)
	{
		if (const UAvaTransitionSubsystem* TransitionSubsystem = GetTransitionSubsystem(InPlayable))
		{
			return GetTransitionBehavior(InPlayable, *TransitionSubsystem);
		}
		return nullptr;		
	}

	bool MakeNullTransitionBehaviorInstance(UAvalanchePlayableTransition* InPlayableTransition, const FAvaTagHandle& InTransitionLayer, FAvaTransitionBehaviorInstance& OutBehaviorInstance)
	{
		IAvaTransitionBehavior* const TransitionBehavior = nullptr;
		OutBehaviorInstance = FAvaTransitionBehaviorInstance();
		OutBehaviorInstance.SetBehavior(TransitionBehavior);
		OutBehaviorInstance.CreateScene<FAvaTransitionPlayableScene>(InPlayableTransition, InTransitionLayer, InPlayableTransition);
		return true;
	}

	FAvaTagHandle GetTransitionLayer(const FAvaTransitionBehaviorInstance& InBehaviorInstance)
	{
		// Remark: InBehaviorInstance.GetTransitionLayer() doesn't return a valid layer.
		if (const IAvaTransitionBehavior* Behavior = InBehaviorInstance.GetBehavior())
		{
			if (const UAvaTransitionTree* TransitionTree = Behavior->GetTransitionTree())
			{
				return TransitionTree->GetTransitionLayer();
			}
		}
		return FAvaTagHandle();
	}
	
	class FBuilderHelper
	{
	public:
		FBuilderHelper(FString&& InContextName, UAvalanchePlayableTransition* InPlayableTransition)
			: ContextName(MoveTemp(InContextName))
			, PlayableTransition(InPlayableTransition)
		{
			ExecutorBuilder.SetContextName(ContextName);
		}

		const FString& GetContextName() const { return ContextName; }

		bool HasBehaviorInstances() const
		{
			return NumEnterInstance > 0 || NumExitInstance > 0;
		}

		bool AddTransitionBehaviorInstance(UAvalanchePlayable* InPlayable, EAvaMediaPlayableTransitionEntryRole InPlayableRole)
		{
			if (!InPlayable)
			{
				UE_LOG(LogAvalanchePlayable, Error, TEXT("Playable Transition \"%s\" setup error: Invalid playable."), *ContextName);
				return false;
			}

			UAvaTransitionSubsystem* TransitionSubsystem = GetTransitionSubsystem(*InPlayable); 

			if (!TransitionSubsystem)
			{
				UE_LOG(LogAvalanchePlayable, Error,
					TEXT("Playable Transition \"%s\" setup error: Can't retrieve transition subsystem for playable {%s}."),
					*ContextName, *GetPrettyPlayableInfo(InPlayable));
				return false;
			}

			if (LastTransitionSubsystem != nullptr && LastTransitionSubsystem != TransitionSubsystem)
			{
				// Todo: if we hit this error, we need to create multiple playable transitions batched per subsystem.
				UE_LOG(LogAvalanchePlayable, Error,
					TEXT("Playable Transition \"%s\" setup error: Playable {%s} is in a different subsystem."),
					*ContextName, *GetPrettyPlayableInfo(InPlayable));
				return false;
			}
			
			LastTransitionSubsystem = TransitionSubsystem;

			FAvaTransitionBehaviorInstance BehaviorInstance;
			if (MakeTransitionBehaviorInstance(InPlayable, TransitionSubsystem, BehaviorInstance))
			{
				if (InPlayableRole == EAvaMediaPlayableTransitionEntryRole::Enter)
				{
					ExecutorBuilder.AddEnterInstance(BehaviorInstance);
					++NumEnterInstance;
				}
				else
				{
					// Either "playing" or "exit" are considered Exit in that implementation layer.
					ExecutorBuilder.AddExitInstance(BehaviorInstance);
					++NumExitInstance;
					
					// Special case of transition out (no enter instances).
					// The way this works, we need to make a dummy "Enter" behavior instance
					// to provide the transition layer for the logic take out.
					if (NumEnterInstance == 0 && InPlayableRole == EAvaMediaPlayableTransitionEntryRole::Exit)
					{
						FAvaTransitionBehaviorInstance NullEnterInstance;
						if (MakeNullTransitionBehaviorInstance(PlayableTransition, GetTransitionLayer(BehaviorInstance), NullEnterInstance))
						{
							ExecutorBuilder.AddEnterInstance(NullEnterInstance);
						}
					}
				}
				return true;
			}
			return false;
		}
		
		bool MakeTransitionBehaviorInstance(UAvalanchePlayable* InPlayable, const UAvaTransitionSubsystem* InTransitionSubsystem, FAvaTransitionBehaviorInstance& OutBehaviorInstance) const
		{
			if (IAvaTransitionBehavior* TransitionBehavior = GetTransitionBehavior(*InPlayable, *InTransitionSubsystem))
			{
				if (const UAvaTransitionTree* TransitionTree = TransitionBehavior->GetTransitionTree())
				{
					if (TransitionTree->IsEnabled())
					{
						OutBehaviorInstance = FAvaTransitionBehaviorInstance();
						OutBehaviorInstance.SetBehavior(TransitionBehavior);
						OutBehaviorInstance.CreateScene<FAvaTransitionPlayableScene>(InPlayable, InPlayable, PlayableTransition);
						return true;
					}
				}
			}
			return false;
		}

	private:
		FString ContextName;
		UAvalanchePlayableTransition* PlayableTransition = nullptr;
		int32 NumEnterInstance = 0;
		int32 NumExitInstance = 0;

	public:
		UAvaTransitionSubsystem* LastTransitionSubsystem = nullptr;
		FAvaTransitionExecutorBuilder ExecutorBuilder;
	};

	struct FSequenceHelper
	{
		const IAvaSequenceProvider* SequenceProvider = nullptr;
		IAvaSequencePlaybackObject* SequencePlayback = nullptr;

		FSequenceHelper(const UAvalanchePlayable* InPlayable)
		{
			if (InPlayable)
			{
				if (IAvaSceneInterface* SceneInterface = InPlayable->GetSceneInterface())
				{
					SequenceProvider = SceneInterface->GetSequenceProvider();
					SequencePlayback = SceneInterface->GetPlaybackObject();
				}
			}
		}

		bool IsValid() const
		{
			return SequenceProvider && SequencePlayback;
		}
	};

	// Source: FAvaTransitionInitializeSequence::ExecuteSequenceTask
	void InitializeSequences(const UAvalanchePlayable* InPlayable)
	{
		const FSequenceHelper Helper(InPlayable);
		if (Helper.IsValid())
		{
			FAvaSequencePlayParams PlaySettings;
			PlaySettings.Start = PlaySettings.End = FAvaSequenceTime(0.0);
			PlaySettings.PlayMode = EAvaSequencePlayMode::Forward;

			for (const TObjectPtr<UAvaSequence>& Sequence : Helper.SequenceProvider->GetSequences())
			{
				Helper.SequencePlayback->PlaySequence(Sequence.Get(), PlaySettings);
			}
		}
	}	

	// Source: FAvaTransitionSequenceUtils::UpdatePlayerRunStatus
	bool HasActiveSequences(const UAvalanchePlayable* InPlayable)
	{
		const FSequenceHelper Helper(InPlayable);
		if (Helper.IsValid())
		{
			for (const TObjectPtr<UAvaSequence>& Sequence : Helper.SequenceProvider->GetSequences())
			{
				if (const UAvaSequencePlayer* SequencePlayer = Helper.SequencePlayback->GetSequencePlayer(Sequence.Get()))
				{
					// Considering paused as not active.
					if (SequencePlayer->GetPlaybackStatus() != EMovieScenePlayerStatus::Paused)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	bool HasActiveSequences(const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak)
	{
		for (const TWeakObjectPtr<UAvalanchePlayable> PlayableWeak : InPlayablesWeak)
		{
			if (HasActiveSequences(PlayableWeak.Get()))
			{
				return true;
			}
		}
		return false;
	}
}

bool UAvaLocalPlayableTransition::Start()
{
	if (!Super::Start())
	{
		return false;
	}

	using namespace UE::AvaPlayableTransition::Private;

	FBuilderHelper Helper(GetFullName(), this);

	TArray<UAvalanchePlayable*> EnterPlayables = Pin(EnterPlayablesWeak);
	TArray<UAvalanchePlayable*> PlayingPlayables = Pin(PlayingPlayablesWeak);
	TArray<UAvalanchePlayable*> ExitPlayables = Pin(ExitPlayablesWeak);
	
	if (EnterPlayables.IsEmpty() && PlayingPlayables.IsEmpty() && ExitPlayables.IsEmpty())
	{
		UE_LOG(LogAvalanchePlayable, Error,
			TEXT("Playable Transition \"%s\" setup error: no playables specified, either as enter or exit. Nothing to do transition on."),
			*Helper.GetContextName());
		return false;
	}

	int32 ArrayIndex = 0;
	for (UAvalanchePlayable* Playable : EnterPlayables)
	{
		if (EnterPlayableValues.IsValidIndex(ArrayIndex) && EnterPlayableValues[ArrayIndex].IsValid())
		{
			Playable->UpdateRemoteControlCommand(EnterPlayableValues[ArrayIndex].ToSharedRef());	
		}
		
		if (!Helper.AddTransitionBehaviorInstance(Playable, EAvaMediaPlayableTransitionEntryRole::Enter))
		{
			// Handle Playables with no transition tree.
			InitializeSequences(Playable);
			PostExecutorSequencePlayablesWeak.Add(Playable);
		}
		++ArrayIndex;
	}

	for (UAvalanchePlayable* Playable : PlayingPlayables)
	{
		if (!Helper.AddTransitionBehaviorInstance(Playable, EAvaMediaPlayableTransitionEntryRole::Playing))
		{
			// No Transition Tree:
			// Stop exit/playing playable without any transition.
			// We can't integrate that with a transition tree for enter pages atm.
			UAvalanchePlayable::OnTransitionEvent().Broadcast(Playable, this, EAvalanchePlayableTransitionEventFlags::StopPlayable);
		}
	}

	for (UAvalanchePlayable* Playable : ExitPlayables)
	{
		if (!Helper.AddTransitionBehaviorInstance(Playable, EAvaMediaPlayableTransitionEntryRole::Exit))
		{
			// No Transition Tree:
			// Stop exit/playing playable without any transition.
			// We can't integrate that with a transition tree for enter pages atm.
			UAvalanchePlayable::OnTransitionEvent().Broadcast(Playable, this, EAvalanchePlayableTransitionEventFlags::StopPlayable);
		}
	}

	bool bTransitionExecutorStarted = false;
	if (Helper.HasBehaviorInstances() && Helper.LastTransitionSubsystem)
	{
		using namespace UE::AvaMediaPlayback::Utils;
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable Transition \"%s\" starting."), *GetBriefFrameInfo(), *Helper.GetContextName());

		Helper.ExecutorBuilder.SetOnFinished(FSimpleDelegate::CreateUObject(this, &UAvaLocalPlayableTransition::OnTransitionExecutorEnded));
		
		TransitionExecutor = Helper.ExecutorBuilder.Build(*Helper.LastTransitionSubsystem);
		if (TransitionExecutor)
		{
			TransitionExecutor->Start();
			bTransitionExecutorStarted = true;
		}
	}

	// If no transition tree, skip to next phase of the transition.
	if (!bTransitionExecutorStarted)
	{
		PostTransitionExecutorPhase();
	}

	// Todo: reconsider if this is needed and if so where it should go. Not used in code for now.
	UAvalanchePlayable::OnTransitionEvent().Broadcast(nullptr, this, EAvalanchePlayableTransitionEventFlags::Starting);

	return true;
}

void UAvaLocalPlayableTransition::Stop()
{
	if (TransitionExecutor.IsValid())
	{
		TransitionExecutor->Stop();
		
		// Stop should've called OnTransitionTreeEnded, which resets the ptr.
		ensure(!TransitionExecutor.IsValid());
	}

	UAvalanchePlayable::OnSequenceEvent().RemoveAll(this);

	Super::Stop();
}

bool UAvaLocalPlayableTransition::IsRunning() const
{
	// The transition can be considered "running" either if it is executing
	// a transition tree or if it has "post transition tree" sequences going.
	return TransitionExecutor.IsValid() || bWaitOnPostExecutorSequences;
}

void UAvaLocalPlayableTransition::Tick(double InDeltaSeconds)
{
	using namespace UE::AvaPlayableTransition::Private;
	
	// We need to poll the sequences. Sequence Events are not reliable.
	if (bWaitOnPostExecutorSequences)
	{
		if (!HasActiveSequences(PostExecutorSequencePlayablesWeak))
		{
			FinishWaitOnPostExecutorSequences();
			NotifyTransitionFinished();
		}
	}
}

void UAvaLocalPlayableTransition::OnTransitionExecutorEnded()
{
	using namespace UE::AvaMediaPlayback::Utils;
	
	if (TransitionExecutor.IsValid())
	{
		// Notify all the playable that have been discarded.
		auto StopDiscardedPlayables = [this](const TArray<TWeakObjectPtr<UAvalanchePlayable>>& InPlayablesWeak)
		{
			for (const TWeakObjectPtr<UAvalanchePlayable>& PlayableWeak : InPlayablesWeak)
			{
				if (UAvalanchePlayable* Playable = PlayableWeak.Get())
				{
					if (DiscardPlayablesWeak.Contains(Playable))
					{
						UAvalanchePlayable::OnTransitionEvent().Broadcast(Playable, this, EAvalanchePlayableTransitionEventFlags::StopPlayable);
					}
				}
			}
		};

		StopDiscardedPlayables(PlayingPlayablesWeak);
		StopDiscardedPlayables(ExitPlayablesWeak);

		TransitionExecutor.Reset();
		
		UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Transition Executor \"%s\" ended."), *GetBriefFrameInfo(), *GetFullName());
	}

	PostTransitionExecutorPhase();
}

void UAvaLocalPlayableTransition::PostTransitionExecutorPhase()
{
	bool bSequenceStarted = false;
	// Deal with non-TL enter playables. After the TL pages have been taken out,
	// we need to start the sequences.
	for (const TWeakObjectPtr<UAvalanchePlayable>& EnterPlayableWeak : PostExecutorSequencePlayablesWeak)
	{
		if (UAvalanchePlayable* Playable = EnterPlayableWeak.Get())
		{
			// Default settings will play all sequences.
			if (Playable->ExecuteAnimationCommand(EAvaMediaAnimAction::Play, FAnimPlaySettings()) == EAvalanchePlayableCommandResult::Executed)
			{
				bSequenceStarted = true;
			}
		}
	}

	if (bSequenceStarted)
	{
		StartWaitOnPostExecutorSequences();
	}
	else
	{
		NotifyTransitionFinished();
	}
}

void UAvaLocalPlayableTransition::NotifyTransitionFinished()
{
	using namespace UE::AvaMediaPlayback::Utils;
	UE_LOG(LogAvalanchePlayable, Verbose, TEXT("%s Playable Transition \"%s\" ended."), *GetBriefFrameInfo(), *GetFullName());

	// This will indicate the playable transition is completed and can be cleaned up.
	// Combo templates break the one page one playable rule, so we need a dedicated event to signal the end of the playable transition.
	UAvalanchePlayable::OnTransitionEvent().Broadcast(nullptr, this, EAvalanchePlayableTransitionEventFlags::Finished);
}

void UAvaLocalPlayableTransition::StartWaitOnPostExecutorSequences()
{
	bWaitOnPostExecutorSequences = true;

	// Make sure we listen to sequence events.
	if (UAvalanchePlayable::OnSequenceEvent().IsBoundToObject(this))
	{
		UAvalanchePlayable::OnSequenceEvent().AddUObject(this, &UAvaLocalPlayableTransition::OnPlayableSequenceEvent);
	}
}

void UAvaLocalPlayableTransition::FinishWaitOnPostExecutorSequences()
{
	bWaitOnPostExecutorSequences = false;
	
	// Remark: This is the only task of playable transition requiring to listen to sequence events, *for now*.
	UAvalanchePlayable::OnSequenceEvent().RemoveAll(this);
}

void UAvaLocalPlayableTransition::OnPlayableSequenceEvent(UAvalanchePlayable* InPlayable, const FName& SequenceName, EAvalanchePlayableSequenceEventType InEventType)
{
	// Remark: the sequence events are not entirely reliable.
	// The transitions are also "ticked" to poll this condition.
	using namespace UE::AvaPlayableTransition::Private;

	if (PostExecutorSequencePlayablesWeak.Contains(InPlayable) && InEventType == EAvalanchePlayableSequenceEventType::Finished)
	{
		// Check if this was the last active sequence of this transition.
		if (!HasActiveSequences(PostExecutorSequencePlayablesWeak))
		{
			FinishWaitOnPostExecutorSequences();
			NotifyTransitionFinished();
		}
	}
}