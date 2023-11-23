// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHub.h"

#include "Clients/LiveLinkHubClientsController.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Features/IModularFeatures.h"
#include "LiveLinkHubClient.h"
#include "LiveLinkProvider.h"
#include "LiveLinkSubject.h"
#include "LiveLinkSubjectSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Recording/LiveLinkHubRecordingController.h"
#include "Recording/LiveLinkHubRecordingListController.h"
#include "UI/Window/LiveLinkHubWindowController.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub"

void FLiveLinkHub::Initialize()
{
	LiveLinkProvider = MakeShared<FLiveLinkHubProvider>();
	LiveLinkHubClient = MakeShared<FLiveLinkHubClient>(AsShared());

	CommandExecutor = MakeUnique<FConsoleCommandExecutor>();
	IModularFeatures::Get().RegisterModularFeature(IConsoleCommandExecutor::ModularFeatureName(), CommandExecutor.Get());
	
	IModularFeatures::Get().RegisterModularFeature(ILiveLinkClient::ModularFeatureName, LiveLinkHubClient.Get());

	RecordingController = MakeShared<FLiveLinkHubRecordingController>();
	PlaybackController = MakeShared<FLiveLinkHubPlaybackController>();
	RecordingListController = MakeShared<FLiveLinkHubRecordingListController>(AsShared());
	ClientsController = MakeShared<FLiveLinkHubClientsController>(LiveLinkProvider.ToSharedRef());

	FString LiveLinkHubLayoutIni = GConfig->GetConfigFilename(TEXT("LiveLinkHubLayout"));
	WindowController = MakeShared<FLiveLinkHubWindowController>(FLiveLinkHubWindowInitParams{ LiveLinkHubLayoutIni });
	WindowController->RestoreLayout();

	LiveLinkHubClient->OnStaticDataReceived_AnyThread().AddSP(this, &FLiveLinkHub::OnStaticDataReceived_AnyThread);
	LiveLinkHubClient->OnFrameDataReceived_AnyThread().AddSP(this, &FLiveLinkHub::OnFrameDataReceived_AnyThread);
	LiveLinkHubClient->OnLiveLinkSubjectAdded().AddSP(this, &FLiveLinkHub::OnSubjectAdded);
	LiveLinkHubClient->OnLiveLinkSubjectRemoved().AddSP(this, &FLiveLinkHub::OnSubjectRemoved);

	PlaybackController->Start();
}

FLiveLinkHub::~FLiveLinkHub()
{
	RecordingController.Reset();
	PlaybackController.Reset();

	LiveLinkHubClient->OnLiveLinkSubjectRemoved().RemoveAll(this);
	LiveLinkHubClient->OnLiveLinkSubjectAdded().RemoveAll(this);
	LiveLinkHubClient->OnFrameDataReceived_AnyThread().RemoveAll(this);
	LiveLinkHubClient->OnStaticDataReceived_AnyThread().RemoveAll(this);
}

bool FLiveLinkHub::IsInPlayback() const
{
	return PlaybackController->IsInPlayback();
}

bool FLiveLinkHub::IsRecording() const
{
	return RecordingController->IsRecording();
}
void FLiveLinkHub::Tick()
{
	LiveLinkHubClient->Tick();
}

TSharedRef<SWindow> FLiveLinkHub::GetRootWindow() const
{
	return WindowController->GetRootWindow().ToSharedRef();
}

TSharedPtr<FLiveLinkHubProvider> FLiveLinkHub::GetLiveLinkProvider() const
{
	return LiveLinkProvider;
}

TSharedPtr<FLiveLinkHubClientsController> FLiveLinkHub::GetClientsController() const
{
	return ClientsController;
}

TSharedPtr<FLiveLinkHubRecordingController> FLiveLinkHub::GetRecordingController() const
{
	return RecordingController;
}

TSharedPtr<FLiveLinkHubRecordingListController> FLiveLinkHub::GetRecordingListController() const
{
	return RecordingListController;
}

TSharedPtr<FLiveLinkHubPlaybackController> FLiveLinkHub::GetPlaybackController() const
{
	return PlaybackController;
}

void FLiveLinkHub::OnStaticDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, const FLiveLinkStaticDataStruct& InStaticDataStruct)
{
	if (RecordingController->IsRecording())
	{
		RecordingController->RecordStaticData(InSubjectKey, InRole, InStaticDataStruct);
	}

	FLiveLinkStaticDataStruct StaticDataCopy;
	StaticDataCopy.InitializeWith(InStaticDataStruct);
	LiveLinkProvider->UpdateSubjectStaticData(InSubjectKey.SubjectName, InRole, MoveTemp(StaticDataCopy));
}

void FLiveLinkHub::OnFrameDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, const FLiveLinkFrameDataStruct& InFrameDataStruct)
{
	if (RecordingController->IsRecording())
	{
		RecordingController->RecordFrameData(InSubjectKey, InFrameDataStruct);
	}

	
	FLiveLinkFrameDataStruct FrameDataCopy;
	FrameDataCopy.InitializeWith(InFrameDataStruct);
	LiveLinkProvider->UpdateSubjectFrameData(InSubjectKey.SubjectName, MoveTemp(FrameDataCopy));
}

void FLiveLinkHub::OnSubjectAdded(FLiveLinkSubjectKey InSubjectKey)
{
	// Send an update to connected clients as well.
	ULiveLinkSubjectSettings* SubjectSettings = Cast<ULiveLinkSubjectSettings>(LiveLinkHubClient->GetSubjectSettings(InSubjectKey));

	if (const FLiveLinkStaticDataStruct* StaticData = LiveLinkHubClient->GetSubjectStaticData(InSubjectKey))
	{
		FLiveLinkStaticDataStruct StaticDataCopy;
		StaticDataCopy.InitializeWith(*StaticData);
		LiveLinkProvider->UpdateSubjectStaticData(InSubjectKey.SubjectName, SubjectSettings->Role, MoveTemp(StaticDataCopy));
	}
}

void FLiveLinkHub::OnSubjectRemoved(FLiveLinkSubjectKey InSubjectKey)
{
	// Send an update to connected clients as well.
	LiveLinkProvider->RemoveSubject(InSubjectKey.SubjectName);
}

#undef LOCTEXT_NAMESPACE /*LiveLinkHub*/
