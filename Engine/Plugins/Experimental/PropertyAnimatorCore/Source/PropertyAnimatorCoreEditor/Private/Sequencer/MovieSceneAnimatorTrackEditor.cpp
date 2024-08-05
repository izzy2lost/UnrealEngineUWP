// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAnimatorTrackEditor.h"

#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "MovieSceneAnimatorTrackEditor"

void FMovieSceneAnimatorTrackEditor::BuildAddTrackMenu(FMenuBuilder& InMenuBuilder)
{
	InMenuBuilder.AddMenuEntry(
		LOCTEXT("AddAnimatorTrack.Label", "Animator"),
		LOCTEXT("AddAnimatorTrack.Tooltip", "Adds a new track that uses the time of the current sequence to drive animators."),
		FSlateIconFinder::FindIconForClass(UMovieSceneAnimatorTrack::StaticClass()),
		FUIAction(
			FExecuteAction::CreateSP(this, &FMovieSceneAnimatorTrackEditor::ExecuteAddTrack)
		)
	);
}

void FMovieSceneAnimatorTrackEditor::ExecuteAddTrack()
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
	NewTrack->AddSection(*NewTrack->CreateNewSection());

	FocusedMovieScene->AddGivenTrack(NewTrack);
	SequencerPtr->OnAddTrack(NewTrack, FGuid());
}

#undef LOCTEXT_NAMESPACE
