// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Recording/LiveLinkRecordingPlayer.h"

#include "Recording/Implementations/LiveLinkUAssetRecording.h"

/** Playback track that holds recorded data for a given subject. */
struct FLiveLinkPlaybackTrack
{
	/** Retrieve all frames from last read index to the new playhead, forward looking. */
	void GetFramesUntil(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames);

	/** Retrieve all frames from last read index to the new playhead, reverse looking. */
	void GetFramesUntilReverse(double InPlayhead, TArray<FLiveLinkRecordedFrame>& OutFrames);
	
	/** Retrieve the frame at the read index. */
	bool TryGetFrame(int32 InIndex, FLiveLinkRecordedFrame& OutFrame);

	/**
	 * Convert the playhead time to a frame index.
	 * @param InPlayhead The play head to convert to a frame index.
	 * @return The frame index or INDEX_NONE.
	 */
	int32 PlayheadToFrameIndex(double InPlayhead);

	/**
	 * Convert the frameindex to a playhead.
	 * @return The frame index or INDEX_NONE.
	 */
	double FrameIndexToPlayhead(int32 InIndex);

	/** Reset the LastReadIndex. */
	void Restart(int32 NewIndex = INDEX_NONE)
	{
		LastReadRelativeIndex = NewIndex < FrameData.Num() && NewIndex < Timestamps.Num() ? NewIndex : INDEX_NONE;
		LastReadAbsoluteIndex = LastReadRelativeIndex;
	}

	/** Convert an absolute frame index to a relative frame index. */
	int32 GetRelativeIndex(int32 InAbsoluteIndex) const
	{
		int32 RelativeIndex = InAbsoluteIndex - StartIndexOffset;
		RelativeIndex = FMath::Clamp(RelativeIndex, 0, FrameData.Num() - 1);
		return RelativeIndex;
	}

	/** Frame data to read. */
	TConstArrayView<TSharedPtr<FInstancedStruct>> FrameData;
	/** Timestamps for the frames in the track. */
	TConstArrayView<double> Timestamps;
	/** Used for static data. */
	TSubclassOf<ULiveLinkRole> LiveLinkRole;
	/** Subject key. */
	FLiveLinkSubjectKey SubjectKey;
	/** Index of the last relative frame that was read by the GetFrames method. */
	int32 LastReadRelativeIndex = -1;
	/** Index of the last absolute frame that was read by the GetFrames method. */
	int32 LastReadAbsoluteIndex = -1;
	/** The true index FrameData starts at. IE, if it starts at 5, then there are 5 prior frames [0..4] that aren't loaded. */
	int32 StartIndexOffset = 0;

private:
	/** The last timestamp recorded. */
	double LastTimeStamp = -1.f;

	friend class FLiveLinkPlaybackTrackIterator;
};

/** Reorganized recording data to facilitate playback. */
struct FLiveLinkPlaybackTracks
{
	/** Get the next frames */
	TArray<FLiveLinkRecordedFrame> FetchNextFrames(double Playhead);

	/** Get the previous frames as if going in reverse */
	TArray<FLiveLinkRecordedFrame> FetchPreviousFrames(double Playhead);
	
	/** Get the next frame(s) at the index */
	TArray<FLiveLinkRecordedFrame> FetchNextFramesAtIndex(int32 FrameIndex);

	/** Convert the playhead to a frame index */
	int32 PlayheadToFrameIndex(double InPlayhead);

	/** Convert the index to a playhead */
	double FrameIndexToPlayhead(int32 InIndex);
	
	void Restart(int32 InIndex);

	/** Retrieve the framerate of the first frame */
	FFrameRate GetInitialFrameRate() const;

public:
	/** LiveLink tracks to playback. */
	TMap<FLiveLinkSubjectKey, FLiveLinkPlaybackTrack> Tracks;
};

class FLiveLinkUAssetRecordingPlayer : public ILiveLinkRecordingPlayer
{
public:
	virtual void PreparePlayback(class ULiveLinkRecording* CurrentRecording) override;

	virtual void ShutdownPlayback() override;
	
	virtual TArray<FLiveLinkRecordedFrame> FetchNextFramesAtTimestamp(const FQualifiedFrameTime& InFrameTime) override
	{
		StreamPlayback(InFrameTime.Time.GetFrame().Value);
		return CurrentRecordingPlayback.FetchNextFrames(InFrameTime.AsSeconds());
	}

	virtual TArray<FLiveLinkRecordedFrame> FetchPreviousFramesAtTimestamp(const FQualifiedFrameTime& InFrameTime) override
	{
		StreamPlayback(InFrameTime.Time.GetFrame().Value);
		return CurrentRecordingPlayback.FetchPreviousFrames(InFrameTime.AsSeconds());
	}

	virtual TArray<FLiveLinkRecordedFrame> FetchNextFramesAtIndex(int32 FrameIndex) override
	{
		StreamPlayback(FrameIndex);
		return CurrentRecordingPlayback.FetchNextFramesAtIndex(FrameIndex);
	}

	virtual void RestartPlayback(int32 InIndex) override
	{
		CurrentRecordingPlayback.Restart(InIndex);
	}

	virtual FFrameRate GetInitialFramerate() override
	{
		return CurrentRecordingPlayback.GetInitialFrameRate();
	}

	virtual TRange<int32> GetBufferedFrames() override
	{
		return LoadedRecording.IsValid() ? LoadedRecording->GetBufferedFrames() : TRange<int32>(0, 0);
	}

private:
	/** Buffer playback around a given frame. */
	void StreamPlayback(int32 InFromFrame);

	/** Retrieve the total frames to buffer, based on the size the user specified in the config file. */
	int32 GetNumFramesToBuffer() const;
	
private:
	/** All tracks for the current recording. */
	FLiveLinkPlaybackTracks CurrentRecordingPlayback;

	/** The recording currently loaded. */
	TWeakObjectPtr<ULiveLinkUAssetRecording> LoadedRecording;
};
