// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkHubModule.h"
#include "Templates/SharedPointer.h"

class FLiveLinkHub;
struct ILiveLinkProvider;
class FLiveLinkHubRecordingController;
class FLiveLinkHubPlaybackController;
class FLiveLinkHubRecordingListController;

class FLiveLinkHubModule : public ILiveLinkHubModule
{
public:
	//~ Begin ILiveLinkHubModule interface
	virtual void StartLiveLinkHub() override;
	//~ End ILiveLinkHubModule interface

	/** Get the livelink hub object. */
	TSharedPtr<FLiveLinkHub> GetLiveLinkHub() const;
	/** Get the livelink provider responsible for forwarding livelink data to connected UE clients. */
	TSharedPtr<ILiveLinkProvider> GetLiveLinkProvider() const;
	/** Get the recording controller. */
	TSharedPtr<FLiveLinkHubRecordingController> GetRecordingController() const;
	/** Get the recording list controller. */
	TSharedPtr<FLiveLinkHubRecordingListController> GetRecordingListController() const;
	/** Get the playback controller. */
	TSharedPtr<FLiveLinkHubPlaybackController> GetPlaybackController() const;
private:
	/** LiveLinkHub object responsible for initializing the different controllers. */
	TSharedPtr<FLiveLinkHub> LiveLinkHub;
};
