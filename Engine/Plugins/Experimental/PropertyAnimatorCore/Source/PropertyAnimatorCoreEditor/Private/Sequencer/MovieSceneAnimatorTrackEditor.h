// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "Sequencer/MovieSceneAnimatorTrack.h"

class FMovieSceneAnimatorTrackEditor : public FKeyframeTrackEditor<UMovieSceneAnimatorTrack>
{
public:
	FMovieSceneAnimatorTrackEditor(const TSharedRef<ISequencer>& InSequencer)
		: FKeyframeTrackEditor<UMovieSceneAnimatorTrack>(InSequencer)
	{}

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
	{
		return MakeShared<FMovieSceneAnimatorTrackEditor>(InSequencer);
	}

private:
	//~ Begin FMovieSceneTrackEditor
	virtual void BuildAddTrackMenu(FMenuBuilder& InMenuBuilder) override;
	//~ End FMovieSceneTrackEditor

	void ExecuteAddTrack();
};
