// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkRole.h"
#include "LiveLinkTypes.h"
#include "StructView.h"
#include "Templates/SubclassOf.h"

class ULiveLinkRecording;

/** Frame that was read by the recording player*/
struct FLiveLinkRecordedFrame
{
	/** Recorded frame or static data. */
	FConstStructView Data;
	/** Subject that originally sent the data. */
	FLiveLinkSubjectKey SubjectKey;
	/** Role used to interpret the data (Only present with recorded static data). */
	TSubclassOf<ULiveLinkRole> LiveLinkRole;
};

/** Object responsible for reading a livelink recording and providing the frames to the LiveLinkPlaybackController. */
class ILiveLinkRecordingPlayer
{
public:
	virtual ~ILiveLinkRecordingPlayer() = default;

	/** Initialize internal structures needed for playback of the recorded data. */
	virtual void PreparePlayback(const ULiveLinkRecording* Recording) = 0;

	/** Fetch next frames at the provided playhead position. */
	virtual TArray<FLiveLinkRecordedFrame> FetchNextFrames(double Playhead) = 0;

	/** Restart the recording from the beginning. */
	virtual void RestartPlayback() = 0;
};
