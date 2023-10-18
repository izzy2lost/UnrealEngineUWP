// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkTypes.h"
#include "Templates/SubclassOf.h"

class FLiveLinkHubClient;
class FLiveLinkHubPlaybackController;
class FLiveLinkHubRecordingController;
class FLiveLinkHubRecordingListController;
class FLiveLinkHubWindowController;
struct FLiveLinkSubjectKey;
struct ILiveLinkProvider;
class SWindow;
class ULiveLinkRole;

/**
 * Main interface for the live link hub.
 */
class ILiveLinkHub
{
public:
	virtual ~ILiveLinkHub() {}
	// todo: replace with GetStatus?

	/** Whether the hub is currently playing a recording. */
	virtual bool IsInPlayback() const = 0;
	/** Whether the hub is currently recording livelink data. */
	virtual bool IsRecording() const = 0;
};

/**
 * Implementation of the live link hub.
 * Contains the apps' different components and is responsible for handling communication between them.
 */
class FLiveLinkHub : public ILiveLinkHub, public TSharedFromThis<FLiveLinkHub>
{
public:
	virtual ~FLiveLinkHub();

	//~ Begin ILiveLinkHub interface
	virtual bool IsInPlayback() const override;
	virtual bool IsRecording() const override;
	//~ End ILiveLinkHub interface

public:
	/** Launch the slate application and initialize its components. */
	void Initialize();
	/** Tick the hub. */
	void Tick();
	/** Get the root window that hosts the hub's slate application. */
	TSharedRef<SWindow> GetRootWindow() const;
	/** Get the livelink provider used to rebroadcast livelink data to connected UE clients. */
	TSharedPtr<ILiveLinkProvider> GetLiveLinkProvider() const;
	/** Get the controller that manages recording livelink data. */
	TSharedPtr<FLiveLinkHubRecordingController> GetRecordingController() const;
	/** Get the recording list controller, that handles displaying livelink recording assets. */
	TSharedPtr<FLiveLinkHubRecordingListController> GetRecordingListController() const;
	/** Get the controller that manages playing back livelink data. */
	TSharedPtr<FLiveLinkHubPlaybackController> GetPlaybackController() const;
	
private:
	//~ LiveLink Client delegates
	void OnStaticDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, const FLiveLinkStaticDataStruct& InStaticDataStruct);
	void OnFrameDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, const FLiveLinkFrameDataStruct& InFrameDataStruct);
	void OnSubjectAdded(FLiveLinkSubjectKey InSubjectKey);
	void OnSubjectRemoved(FLiveLinkSubjectKey InSubjectKey);
	//~ LiveLink Client delegates

private:
	/** Recording controller. */
	TSharedPtr<FLiveLinkHubRecordingController> RecordingController;
	/** Recordings list controller. */
	TSharedPtr<FLiveLinkHubRecordingListController> RecordingListController;
	/** Playback controller. */
	TSharedPtr<FLiveLinkHubPlaybackController> PlaybackController;
	/** Window controller */
	TSharedPtr<FLiveLinkHubWindowController> WindowController;
	/** LiveLinkHub's livelink client. */
	TSharedPtr<FLiveLinkHubClient> LiveLinkHubClient;
	/** LiveLinkProvider used to transfer data to connected UE clients. */
	TSharedPtr<ILiveLinkProvider> LiveLinkProvider;

	friend class FLiveLinkHubModule;
};
