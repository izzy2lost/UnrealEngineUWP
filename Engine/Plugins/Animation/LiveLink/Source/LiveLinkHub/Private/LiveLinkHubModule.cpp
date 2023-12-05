// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubModule.h"

#include "Clients/LiveLinkHubProvider.h"
#include "LiveLinkHubApplication.h"
#include "Modules/ModuleManager.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Recording/LiveLinkHubRecordingController.h"

void FLiveLinkHubModule::StartLiveLinkHub()
{
	// Nothing will get executed after this, so put everything before.
	LiveLinkHub = MakeShared<FLiveLinkHub>();
	LiveLinkHub->Initialize();

	LiveLinkHubLoop(LiveLinkHub);

	// If we got to this point, the app is shutdown so we should release our references.
	LiveLinkHub.Reset();
}

TSharedPtr<FLiveLinkHub> FLiveLinkHubModule::GetLiveLinkHub() const
{
	return LiveLinkHub;
}

TSharedPtr<FLiveLinkHubProvider> FLiveLinkHubModule::GetLiveLinkProvider() const
{
	return LiveLinkHub->LiveLinkProvider;
}

TSharedPtr<FLiveLinkHubRecordingController> FLiveLinkHubModule::GetRecordingController() const
{
	return LiveLinkHub->RecordingController;
}

TSharedPtr<FLiveLinkHubRecordingListController> FLiveLinkHubModule::GetRecordingListController() const
{
	return LiveLinkHub->RecordingListController;
}

TSharedPtr<FLiveLinkHubPlaybackController> FLiveLinkHubModule::GetPlaybackController() const
{
	return LiveLinkHub->PlaybackController;
}

TSharedPtr<FLiveLinkHubSubjectController> FLiveLinkHubModule::GetSubjectController() const
{
	return LiveLinkHub->SubjectController;
}

TSharedPtr<ILiveLinkHubSessionManager> FLiveLinkHubModule::GetSessionManager() const
{
	return LiveLinkHub->SessionManager;
}

IMPLEMENT_MODULE(FLiveLinkHubModule, LiveLinkHub);
