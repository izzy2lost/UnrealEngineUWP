// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "Sequencer/MovieSceneAnimatorTrack.h"

/** Animator track editor to add animator track and section */
class FMovieSceneAnimatorTrackEditor : public FKeyframeTrackEditor<UMovieSceneAnimatorTrack>
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAddAnimatorTrack, uint8 /** Channel */)
	static FOnAddAnimatorTrack OnAddAnimatorTrack;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGetAnimatorTrackCount, uint8 /** Channel */, int32& /** Count */)
	static FOnGetAnimatorTrackCount OnGetAnimatorTrackCount;

	FMovieSceneAnimatorTrackEditor(const TSharedRef<ISequencer>& InSequencer)
		: FKeyframeTrackEditor<UMovieSceneAnimatorTrack>(InSequencer)
	{}

	virtual ~FMovieSceneAnimatorTrackEditor() override;

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
	{
		TSharedRef<FMovieSceneAnimatorTrackEditor> TrackEditor = MakeShared<FMovieSceneAnimatorTrackEditor>(InSequencer);
		TrackEditor->BindDelegates();
		return TrackEditor;
	}

private:
	//~ Begin FMovieSceneTrackEditor
	virtual void BuildAddTrackMenu(FMenuBuilder& InMenuBuilder) override;
	//~ End FMovieSceneTrackEditor

	void BindDelegates();

	void GetTrackCount(uint8 InChannel, int32& OutCount) const;

	void ExecuteAddTrack(uint8 InChannel);
};
