// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "KeyframeTrackEditor.h"
#include "Tracks/MovieSceneTimeWarpTrack.h"


class FTimeWarpTrackEditor
	: public FKeyframeTrackEditor<UMovieSceneTimeWarpTrack>
{
public:

	FTimeWarpTrackEditor(TSharedRef<ISequencer> InSequencer)
		: FKeyframeTrackEditor<UMovieSceneTimeWarpTrack>(InSequencer)
	{}

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
	{
		return MakeShared<FTimeWarpTrackEditor>(InSequencer);
	}

private:
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;

	void HandleAddTimeWarpTrack();
};
