// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Recording/LiveLinkRecordingPlayer.h"

#include "Recording/Implementations/LiveLinkUAssetRecording.h"

/** Playback track that holds recorded data for a given subject. */
struct FLiveLinkPlaybackTrack
{
	void GetFramesUntil(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames);

	void Restart()
	{
		LastReadIndex = -1;
	}

	/** Frame data to read. */
	TConstArrayView<struct FInstancedStruct> FrameData;
	/** Timestamps for the frames in the track. */
	TConstArrayView<double> Timestamps;
	/** Used for static data. */
	TSubclassOf<ULiveLinkRole> LiveLinkRole;
	/** Subject key. */
	FLiveLinkSubjectKey SubjectKey;
	/** Index of the last frame that was read by the GetFrames method. */
	int32 LastReadIndex = -1;

	friend class FLiveLinkPlaybackTrackIterator;
};

/** Reorganized recording data to facilitate playback. */
struct FLiveLinkPlaybackTracks
{
	/** Get the next frames */
	TArray<FLiveLinkRecordedFrame> FetchNextFrames(double Playhead);

	void Restart();

public:
	/** LiveLink tracks to playback. */
	TArray<FLiveLinkPlaybackTrack> Tracks;
};

class FLiveLinkUAssetRecordingPlayer : public ILiveLinkRecordingPlayer
{
public:
	void PreparePlayback(const class ULiveLinkRecording* CurrentRecording);

	virtual TArray<FLiveLinkRecordedFrame> FetchNextFrames(double Playhead) override
	{
		return CurrentRecordingPlayback.FetchNextFrames(Playhead);
	}

	virtual void RestartPlayback() override
	{
		CurrentRecordingPlayback.Restart();
	}
	
private:
	/** All tracks for the current recording. */
	FLiveLinkPlaybackTracks CurrentRecordingPlayback;
};
