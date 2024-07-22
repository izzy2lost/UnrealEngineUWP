// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/AvaPropertyAnimatorSequenceTimeSource.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "AvaSceneSubsystem.h"
#include "AvaSequence.h"
#include "AvaSequencePlayer.h"
#include "IAvaSceneInterface.h"
#include "IAvaSequenceProvider.h"
#include "MovieSceneSequence.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#endif // WITH_EDITOR

FName UAvaPropertyAnimatorSequenceTimeSource::GetSequenceName(const UAvaSequence* InSequence)
{
	if (!InSequence)
	{
		return NAME_None;
	}

	return FName(InSequence->GetLabel().ToString() + TEXT(" (") + InSequence->GetName() + TEXT(")"));
}

#if WITH_EDITOR
void UAvaPropertyAnimatorSequenceTimeSource::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	static const FName SequenceNamePropertyName = GET_MEMBER_NAME_CHECKED(UAvaPropertyAnimatorSequenceTimeSource, SequenceName);

	if (MemberName == SequenceNamePropertyName)
	{
		OnSequenceChanged();
	}
}
#endif // WITH_EDITOR

void UAvaPropertyAnimatorSequenceTimeSource::SetSequenceName(FName InSequenceName)
{
	if (SequenceName == InSequenceName)
	{
		return;
	}

	const TArray<FName> Sequences = GetSequenceNames();
	if (!Sequences.Contains(InSequenceName))
	{
		return;
	}

	SequenceName = InSequenceName;
	OnSequenceChanged();
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceActive()
{
	Super::OnTimeSourceActive();

	OnSequenceChanged();
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceInactive()
{
	Super::OnTimeSourceInactive();

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);

	SequencePlayerWeak.Reset();
	SequencePlayerWeak = nullptr;
}

#if WITH_EDITOR
void UAvaPropertyAnimatorSequenceTimeSource::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	SequencerWeak = InSequencer;
}

TSharedPtr<ISequencer> UAvaPropertyAnimatorSequenceTimeSource::GetSequencer() const
{
	if (const UAvaPropertyAnimatorSequenceTimeSource* Default = GetDefault<UAvaPropertyAnimatorSequenceTimeSource>())
	{
		return Default->SequencerWeak.Pin();
	}

	return nullptr;
}
#endif // WITH_EDITOR

double UAvaPropertyAnimatorSequenceTimeSource::GetTimeElapsed()
{
	// Valid when a sequence player is playing
	if (const UAvaSequencePlayer* SequencePlayer = SequencePlayerWeak.Get())
	{
		return SequencePlayer->GetCurrentTime().AsSeconds();
	}

#if WITH_EDITOR
	// Use sequencer global time if sequencer active
	if (const TSharedPtr<ISequencer> Sequencer = GetSequencer())
	{
		// Scrub to global time only if root sequence is the selected sequence
		if (const UAvaSequence* RootSequence = Cast<UAvaSequence>(Sequencer->GetRootMovieSceneSequence()))
		{
			if (GetSequenceName(RootSequence) == SequenceName)
			{
				return Sequencer->GetGlobalTime().AsSeconds();
			}
		}
	}
#endif

	return 0;
}

bool UAvaPropertyAnimatorSequenceTimeSource::IsTimeSourceReady() const
{
	return SequencePlayerWeak.IsValid()
#if WITH_EDITOR
		|| GetSequencer()
#endif
	;
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceRegistered()
{
	Super::OnTimeSourceRegistered();

#if WITH_EDITOR
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	OnSequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateUObject(this, &UAvaPropertyAnimatorSequenceTimeSource::OnSequencerCreated));
#endif // WITH_EDITOR
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceUnregistered()
{
	Super::OnTimeSourceUnregistered();

#if WITH_EDITOR
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.UnregisterOnSequencerCreated(OnSequencerCreatedHandle);
	OnSequencerCreatedHandle.Reset();
#endif // WITH_EDITOR
}

TArray<FName> UAvaPropertyAnimatorSequenceTimeSource::GetSequenceNames() const
{
	TArray<FName> SequenceNames {NAME_None};

	for (const UAvaSequence* Sequence : GetSequences())
	{
		const FName MovieSceneSequenceName = GetSequenceName(Sequence);

		if (!MovieSceneSequenceName.IsNone())
		{
			SequenceNames.Add(MovieSceneSequenceName);
		}
	}

	return SequenceNames;
}

TArray<UAvaSequence*> UAvaPropertyAnimatorSequenceTimeSource::GetSequences() const
{
	TArray<UAvaSequence*> Sequences;

	const UPropertyAnimatorCoreBase* Animator = GetAnimator();
	if (!Animator)
	{
		return Sequences;
	}

	const AActor* AnimatorActor = Animator->GetAnimatorActor();
	if (!AnimatorActor)
	{
		return Sequences;
	}

	ULevel* AnimatorLevel = AnimatorActor->GetLevel();
	if (!AnimatorLevel)
	{
		return Sequences;
	}

	const IAvaSceneInterface* SceneInterface = UAvaSceneSubsystem::FindSceneInterface(AnimatorLevel);
	if (!SceneInterface)
	{
		return Sequences;
	}

	const IAvaSequenceProvider* SequenceProvider = SceneInterface->GetSequenceProvider();
	if (!SequenceProvider)
	{
		return Sequences;
	}

	// Find sequence with that name
	for (const TWeakObjectPtr<UAvaSequence>& RootSequenceWeak : SequenceProvider->GetRootSequences())
	{
		if (UAvaSequence* RootSequence = RootSequenceWeak.Get())
		{
			Sequences.Add(RootSequence);
		}
	}

	return Sequences;
}

void UAvaPropertyAnimatorSequenceTimeSource::OnSequenceChanged()
{
	// Reset sequence selection
	if (SequenceName.IsNone())
	{
		SequenceWeak = nullptr;
		SequenceWeak.Reset();

		SequencePlayerWeak = nullptr;
		SequencePlayerWeak.Reset();

		UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
		UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
		UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);

		return;
	}

	// Find sequence with that name
	for (UAvaSequence* Sequence : GetSequences())
	{
		if (Sequence && GetSequenceName(Sequence) == SequenceName)
		{
			SequenceWeak = Sequence;
			break;
		}
	}

	if (!SequenceWeak.IsValid())
	{
		return;
	}

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceStarted().AddUObject(this, &UAvaPropertyAnimatorSequenceTimeSource::OnSequenceStarted);
}

void UAvaPropertyAnimatorSequenceTimeSource::OnSequenceStarted(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence)
{
	if (!InPlayer || !InSequence)
	{
		return;
	}

	const UAvaSequence* Sequence = SequenceWeak.Get();
	if (InSequence != Sequence)
	{
		return;
	}

	SequencePlayerWeak = InPlayer;

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceFinished().AddUObject(this, &UAvaPropertyAnimatorSequenceTimeSource::OnSequenceFinished);
}

void UAvaPropertyAnimatorSequenceTimeSource::OnSequenceFinished(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence)
{
	if (!InPlayer || !InSequence)
	{
		return;
	}

	const UAvaSequence* Sequence = SequenceWeak.Get();
	if (Sequence != InSequence)
	{
		return;
	}

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	SequencePlayerWeak = nullptr;
	SequencePlayerWeak.Reset();
}
