// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAnimatorTrackEditor.h"

#include "Sequencer/MovieSceneAnimatorSection.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "MovieSceneAnimatorTrackEditor"

FMovieSceneAnimatorTrackEditor::FOnAddAnimatorTrack FMovieSceneAnimatorTrackEditor::OnAddAnimatorTrack;
FMovieSceneAnimatorTrackEditor::FOnGetAnimatorTrackCount FMovieSceneAnimatorTrackEditor::OnGetAnimatorTrackCount;

FMovieSceneAnimatorTrackEditor::~FMovieSceneAnimatorTrackEditor()
{
	OnAddAnimatorTrack.RemoveAll(this);
	OnGetAnimatorTrackCount.RemoveAll(this);
}

void FMovieSceneAnimatorTrackEditor::BuildAddTrackMenu(FMenuBuilder& InMenuBuilder)
{
	constexpr uint8 Channel = 0;

	InMenuBuilder.AddMenuEntry(
		LOCTEXT("AddAnimatorTrack.Label", "Animator"),
		LOCTEXT("AddAnimatorTrack.Tooltip", "Adds a new track that uses the time of the current sequence to drive animators."),
		FSlateIconFinder::FindIconForClass(UMovieSceneAnimatorTrack::StaticClass()),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMovieSceneAnimatorTrackEditor::ExecuteAddTrack, Channel)
		)
	);
}

void FMovieSceneAnimatorTrackEditor::BindDelegates()
{
	OnAddAnimatorTrack.AddSP(this, &FMovieSceneAnimatorTrackEditor::ExecuteAddTrack);
	OnGetAnimatorTrackCount.AddSP(this, &FMovieSceneAnimatorTrackEditor::GetTrackCount);
}

void FMovieSceneAnimatorTrackEditor::GetTrackCount(uint8 InChannel, int32& OutCount) const
{
	const UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene)
	{
		return;
	}

	for (UMovieSceneTrack* Track : FocusedMovieScene->GetTracks())
	{
		if (const UMovieSceneAnimatorTrack* AnimatorTrack = Cast<UMovieSceneAnimatorTrack>(Track))
		{
			OutCount += AnimatorTrack->GetChannelCount(InChannel);
		}
	}
}

void FMovieSceneAnimatorTrackEditor::ExecuteAddTrack(uint8 InChannel)
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();
	if (!FocusedMovieScene || FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (!SequencerPtr)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddAnimatorTrack", "Add Animator Track"));

	FocusedMovieScene->Modify();

	UMovieSceneAnimatorTrack* NewTrack = NewObject<UMovieSceneAnimatorTrack>(FocusedMovieScene, NAME_None, RF_Transactional);
	UMovieSceneAnimatorSection* NewSection = Cast<UMovieSceneAnimatorSection>(NewTrack->CreateNewSection());
	NewSection->SetChannel(InChannel);
	NewTrack->AddSection(*NewSection);

	FocusedMovieScene->AddGivenTrack(NewTrack);
	SequencerPtr->OnAddTrack(NewTrack, FGuid());
}

#undef LOCTEXT_NAMESPACE
