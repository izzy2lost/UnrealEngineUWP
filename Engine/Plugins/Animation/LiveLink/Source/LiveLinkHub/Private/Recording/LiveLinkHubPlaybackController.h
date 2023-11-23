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

class FLiveLinkHubPlaybackController : public FRunnable
{
public:
	FLiveLinkHubPlaybackController();
	virtual ~FLiveLinkHubPlaybackController() override;
	
	/** Start playing a livelink recording. */
	void PlayRecording(ULiveLinkRecording* InLiveLinkRecording);

	/** Stop playing a livelink recording. */
	void StopPlayback();

	/** Returns whether we've started or are actively playing a recording. */ 
	bool IsInPlayback() const
	{
		return RecordingToPlay || bIsInPlayback.load();
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
	/** Apply the recording's preset then prepare the data needed to playback. */
	void PreparePlayback(ULiveLinkRecording* InLiveLinkRecording);
	/** Handler called when playback is finished on the playback thread. Is responsible for resetting the livelink state to what it was before we started playback. */
	void OnPlaybackFinished_Internal();
	
private:
	/** Flag for terminating the thread loop */
	std::atomic<bool> Stopping = false;
	/** Thread to do playback on **/
	TUniquePtr<FRunnableThread> Thread;
	/** Event signaling that a recording is available for playback. */
	FEventRef PlaybackEvent = FEventRef();
	/** Whether we're doing playback. */
	std::atomic<bool> bIsInPlayback = false;
	/** Recording we're currently playing. */
	TSharedPtr<FLiveLinkRecordingData> CurrentRecording;
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
	double Playhead = 0.0;
};
