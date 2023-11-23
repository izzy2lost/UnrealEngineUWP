// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkUAssetRecordingPlayer.h"

#include "Recording/LiveLinkRecording.h"


class FLiveLinkPlaybackTrackIterator
{
public:
	FLiveLinkPlaybackTrackIterator(FLiveLinkPlaybackTrack& InTrack, int32 InInitialIndex)
		: Track(InTrack)
		, FrameIndex(InInitialIndex)
	{
	}

	/** Advances to the next frame */
	void operator++()
	{
		Advance();
	}

	/* @Return True if there are more frames in this track. */
	explicit operator bool() const
	{
		return HasMoreFrames();
	}

	double FrameTimestamp() const
	{
		return Track.Timestamps[FrameIndex];
	}

	const FInstancedStruct& FrameData() const
	{
		return Track.FrameData[FrameIndex];
	}

	int32 CurrentIndex() const
	{
		return FrameIndex;
	}

private:
	/* @Return True if there are more vertices on the component */
	bool HasMoreFrames() const
	{
		return FrameIndex < Track.Timestamps.Num() && FrameIndex < Track.FrameData.Num();
	}

	/* Advances to the next frame */
	void Advance()
	{
		++FrameIndex;
	}

private:
	/** Track that's currently being iterated. */
	FLiveLinkPlaybackTrack& Track;
	/** "Playhead" for this track */
	int32 FrameIndex = 0;
};

void FLiveLinkPlaybackTrack::GetFramesUntil(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames)
{
	for (auto It = FLiveLinkPlaybackTrackIterator(*this, LastReadIndex + 1); It; ++It)
	{
		if (It.FrameTimestamp() > InPlayhead)
		{
			break;
		}

		LastReadIndex = It.CurrentIndex();

		FLiveLinkRecordedFrame FrameToPlay;
		FrameToPlay.Data = It.FrameData();
		FrameToPlay.SubjectKey = SubjectKey;
		FrameToPlay.LiveLinkRole = LiveLinkRole;

		OutFrames.Add(MoveTemp(FrameToPlay));
	}
}

TArray<FLiveLinkRecordedFrame> FLiveLinkPlaybackTracks::FetchNextFrames(double Playhead)
{
	TArray<FLiveLinkRecordedFrame> NextFrames;

	if (Tracks.Num())
	{
		// todo: sort frames by timestamp
		for (FLiveLinkPlaybackTrack& Track : Tracks)
		{
			Track.GetFramesUntil(Playhead, NextFrames);
		}
	}

	return NextFrames;
}

void FLiveLinkPlaybackTracks::Restart()
{
	for (FLiveLinkPlaybackTrack& Track : Tracks)
	{
		Track.Restart();
	}
}

void FLiveLinkUAssetRecordingPlayer::PreparePlayback(const ULiveLinkRecording* CurrentRecording)
{
	const ULiveLinkUAssetRecording* UAssetRecording = CastChecked<ULiveLinkUAssetRecording>(CurrentRecording);

	FLiveLinkPlaybackTracks RecordingPlayback;

	for (const TPair<FLiveLinkSubjectKey, FLiveLinkRecordingStaticDataContainer>& Pair : UAssetRecording->RecordingData.StaticData)
	{
		FLiveLinkPlaybackTrack PlaybackTrack;
		PlaybackTrack.FrameData = TConstArrayView<FInstancedStruct>(Pair.Value.RecordedData);
		PlaybackTrack.Timestamps = TConstArrayView<double>(Pair.Value.Timestamps);
		PlaybackTrack.LiveLinkRole = Pair.Value.Role;
		PlaybackTrack.SubjectKey = Pair.Key;
		RecordingPlayback.Tracks.Add(PlaybackTrack);
	}

	for (const TPair<FLiveLinkSubjectKey, FLiveLinkRecordingBaseDataContainer>& Pair : UAssetRecording->RecordingData.FrameData)
	{
		FLiveLinkPlaybackTrack PlaybackTrack;
		PlaybackTrack.FrameData = TConstArrayView<FInstancedStruct>(Pair.Value.RecordedData);
		PlaybackTrack.Timestamps = TConstArrayView<double>(Pair.Value.Timestamps);
		PlaybackTrack.SubjectKey = Pair.Key;
		RecordingPlayback.Tracks.Add(PlaybackTrack);
	}

	CurrentRecordingPlayback = MoveTemp(RecordingPlayback);
}
