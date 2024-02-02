// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"

#include "HAL/Event.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/Timespan.h"
#include "Misc/Optional.h"
#include "Recording/LiveLinkRecordingPlayer.h"
#include "Templates/SubclassOf.h"
#include "Templates/SharedPointer.h"
#include "Templates/PimplPtr.h"
#include "UObject/StrongObjectPtr.h"

#include <atomic>

class ILiveLinkClient;
struct FLiveLinkRecordingData;
class FRunnableThread;
class ULiveLinkRecording;
class ULiveLinkPreset;
class ULiveLinkRecording;
class SWidget;

class FLiveLinkHubPlaybackController : public FRunnable
{
public:
	FLiveLinkHubPlaybackController();
	virtual ~FLiveLinkHubPlaybackController() override;

	/** Create the playback widget. */
	TSharedRef<SWidget> MakePlaybackWidget();

	/** Apply the recording's preset then prepare the data needed to playback. */
	void PreparePlayback(ULiveLinkRecording* InLiveLinkRecording);
	
	/** Start playing a livelink recording. */
	void PlayRecording(ULiveLinkRecording* InLiveLinkRecording);

	/** The current recording. */
	const TStrongObjectPtr<ULiveLinkRecording>& GetRecording() const { return RecordingToPlay; }
	
	/** Start playing the currently prepared recording. */
	void BeginPlayback(bool bInReverse);

	/** Prepare to restart the playback. */
	void RestartPlayback();

	/** Pause playback. */
	void PausePlayback();
	
	/** Stop playing a livelink recording. */
	void StopPlayback();

	/** Stop playback and restore the previous settings. */
	void Eject();

	/** Go to a specific time. */
	void GoToTime(double InTime);

	/** Go to a specific frame index. */
	void GoToFrame(int32 InFrameIndex);

	/** Calculate the frame index for the given time. */
	int32 GetFrameIndexFromTime(double InTime, bool bReverse = false) const;

	/** Retrieve the timestamp from the given frame index. */
	double GetTimeFromFrameIndex(int32 InFrameIndex) const;

	/** Retrieve the selection start frame. */
	int32 GetSelectionStartFrame() const;

	/** Retrieve the selection end frame. */
	int32 GetSelectionEndFrame() const;

	/** Retrieve the selection start time. */
	double GetSelectionStartTime() const;

	/** Set the selection start time. */
	void SetSelectionStartTime(double InTime);

	/** Retrieve the selection end time. */
	double GetSelectionEndTime() const;

	/** Set the selection end time. */
	void SetSelectionEndTime(double InTime);

	/** Retrieve the length of the recording. */
	double GetLength() const;

	/** Retrieve the playhead. */
	double GetCurrentTime() const;

	/** Return the exact frame index of frames that have been processed. */
	int32 GetCurrentFrame() const;

	/** If the controller is ready for commands. */
	bool IsReady() const
	{
		return bIsReady;
	}
	
	/** Returns whether we've started or are actively playing a recording. */ 
	bool IsInPlayback() const
	{
		return bIsInPlayback.load();
	}

	/** If playback is paused. */
	bool IsPaused() const
	{
		return bIsPaused.load() || !IsInPlayback();
	}

	/** If playback is playing in reverse. */
	bool IsPlayingInReverse() const
	{
		return bIsReverse.load();
	}

	/** Returns whether the recording is set to loop. */
	bool IsLooping() const
	{
		return bLoopPlayback.load();
	}

	/** Set whether a recording should loop. */
	void SetLooping(bool bInLoop)
	{
		bLoopPlayback.store(bInLoop);
	}
	
	/** Delegate called when playback is finished (if recording is not set to loop). */
	FSimpleMulticastDelegate& OnPlaybackFinished()
	{
		return PlaybackFinishedDelegate;
	}
	
	/** Create the playback thread. */
	void Start();
	
	//~ Begin FRunnable Interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override { }
	//~ End FRunnable Interface

private:
	/** Trigger the playback thread to start reading data. */
	void StartPlayback();
	/** Resume on the playback thread. */
	void ResumePlayback();

	/** Handler called when playback is finished on the playback thread. Is responsible for resetting the livelink state to what it was before we started playback. */
	void OnPlaybackFinished_Internal();

	/**
	 * Send data to the client.
	 * @param NextFrame The frame to push.
	 * @param bForceSync Force the client to sync directly to this frame, discarding all other frames.
	 */
	void PushSubjectData(const FLiveLinkRecordedFrame& NextFrame, bool bForceSync = false);

	/**
	 * Sync the animation to the current playhead value.
	 *
	 * @return true if any frames were pushed.
	 */
	bool SyncToPlayhead();

	/** Force sync to a specific frame. */
	bool SyncToFrame(int32 InFrameIndex);

	/** Checks if the current playback settings indicates the recording should restart. */
	bool ShouldRestart() const;

	/** Retrieve the time delta to use for the recording when dragging the playhead spinbox. */
	double GetTimeDelta() const;
	
private:
	/** If the system has established a connection with the client. */
	bool bIsReady = false;
	/** Flag for terminating the thread loop */
	std::atomic<bool> Stopping = false;
	/** Thread to do playback on **/
	TUniquePtr<FRunnableThread> Thread;
	/** Event signaling that a recording is available for playback. */
	FEventRef PlaybackEvent = FEventRef();
	/** If the playback thread is waiting. */
	std::atomic<bool> bIsPlaybackWaiting = false;
	/** Whether we're doing playback. */
	std::atomic<bool> bIsInPlayback = false;
	/** Whether we're currently paused. */
	std::atomic<bool> bIsPaused = false;
	/** If the recording is playing in reverse. */
	std::atomic<bool>bIsReverse = false;
	/** The timestamp of the animation when first playing. Can be > 0 when running in reverse. */
	std::atomic<double> StartTimestamp = 0.f;
	/** LiveLinkRecording to play.  */
	TStrongObjectPtr<ULiveLinkRecording> RecordingToPlay;
	/** Delegate called when a recording playback is finished (if it's not looping). */
	FSimpleMulticastDelegate PlaybackFinishedDelegate;
	/** Preset used to rollback the hub to its previous state after playing a recording. */
	TStrongObjectPtr<ULiveLinkPreset> RollbackPreset;
	/** Atomic bool keeping track of whether we should loop the playback. */
	std::atomic<bool> bLoopPlayback = false;
	/** Implementation of the playback functionality. */
	TUniquePtr<ILiveLinkRecordingPlayer> RecordingPlayer;

	/** LiveLinkClient used to transmit the data to connected clients. */
	ILiveLinkClient* Client = nullptr;
	/** Time that the playback started. */
	double PlaybackStartTime = 0.0;
	/** Playhead for the current playback. */
	std::atomic<double> Playhead = 0.0;

	/** The view range of the slider, defaults to start/end time. */
	TRange<double> SliderViewRange = TRange<double>(0.f, 0.f);

	/** The playback selection start time. */
	double SelectionStartTime = 0.f;

	/** The playback selection end time. */
	double SelectionEndTime = 0.f;
	
	/** The index of the latest processed frame. This may not exactly match the index calculated from the playhead. */
	int32 CurrentFrameIndex = INDEX_NONE;
};
