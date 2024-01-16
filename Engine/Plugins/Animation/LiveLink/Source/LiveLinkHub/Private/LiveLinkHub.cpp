// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHub.h"

#include "Clients/LiveLinkHubClientsController.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Config/LiveLinkHubFileUtilities.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "LiveLinkHubClient.h"
#include "LiveLinkProvider.h"
#include "LiveLinkHubCommands.h"
#include "LiveLinkProviderImpl.h"
#include "LiveLinkSubject.h"
#include "LiveLinkSubjectSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Recording/LiveLinkHubRecordingController.h"
#include "Recording/LiveLinkHubRecordingListController.h"
#include "Session/LiveLinkHubSessionManager.h"
#include "Subjects/LiveLinkHubSubjectController.h"
#include "UI/Window/LiveLinkHubWindowController.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub"

void FLiveLinkHub::Initialize()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHub::Initialize);

	// We must register the livelink client first since we might rely on the modular feature to initialize the controllers/managers.
	LiveLinkHubClient = MakeShared<FLiveLinkHubClient>(AsShared());
	IModularFeatures::Get().RegisterModularFeature(ILiveLinkClient::ModularFeatureName, LiveLinkHubClient.Get());

	SessionManager = MakeShared<FLiveLinkHubSessionManager>();
	LiveLinkProvider = MakeShared<FLiveLinkHubProvider>(SessionManager.ToSharedRef());

	CommandExecutor = MakeUnique<FConsoleCommandExecutor>();
	IModularFeatures::Get().RegisterModularFeature(IConsoleCommandExecutor::ModularFeatureName(), CommandExecutor.Get());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHub::InitializeControllers);
		RecordingController = MakeShared<FLiveLinkHubRecordingController>();
		PlaybackController = MakeShared<FLiveLinkHubPlaybackController>();
		RecordingListController = MakeShared<FLiveLinkHubRecordingListController>(AsShared());
		ClientsController = MakeShared<FLiveLinkHubClientsController>(LiveLinkProvider.ToSharedRef());
		CommandList = MakeShared<FUICommandList>();
		SubjectController = MakeShared<FLiveLinkHubSubjectController>();
	}


	FLiveLinkHubCommands::Register();
	BindCommands();
	
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

	IModularFeatures::Get().UnregisterModularFeature(ILiveLinkClient::ModularFeatureName, LiveLinkHubClient.Get());
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

TSharedPtr<ILiveLinkHubSessionManager> FLiveLinkHub::GetSessionManager() const
{
	return SessionManager;
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

void FLiveLinkHub::OnStaticDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, TSubclassOf<ULiveLinkRole> InRole, const FLiveLinkStaticDataStruct& InStaticDataStruct) const
{
	if (RecordingController->IsRecording())
	{
		RecordingController->RecordStaticData(InSubjectKey, InRole, InStaticDataStruct);
	}

	FLiveLinkStaticDataStruct StaticDataCopy;
	StaticDataCopy.InitializeWith(InStaticDataStruct);

	const FName OverridenName = GetSubjectNameOverride(InSubjectKey);
	LiveLinkProvider->UpdateSubjectStaticData(OverridenName, InRole, MoveTemp(StaticDataCopy));
}

void FLiveLinkHub::OnFrameDataReceived_AnyThread(const FLiveLinkSubjectKey& InSubjectKey, const FLiveLinkFrameDataStruct& InFrameDataStruct) const
{
	if (RecordingController->IsRecording())
	{
		RecordingController->RecordFrameData(InSubjectKey, InFrameDataStruct);
	}

	
	FLiveLinkFrameDataStruct FrameDataCopy;
	FrameDataCopy.InitializeWith(InFrameDataStruct);

	const FName OverridenName = GetSubjectNameOverride(InSubjectKey);
	if (LiveLinkHubClient->IsSubjectEnabled(OverridenName))
	{
		LiveLinkProvider->UpdateSubjectFrameData(OverridenName, MoveTemp(FrameDataCopy));
	}
}

void FLiveLinkHub::OnSubjectAdded(FLiveLinkSubjectKey InSubjectKey) const
{
	// Send an update to connected clients as well.
	ULiveLinkSubjectSettings* SubjectSettings = Cast<ULiveLinkSubjectSettings>(LiveLinkHubClient->GetSubjectSettings(InSubjectKey));

	if (const FLiveLinkStaticDataStruct* StaticData = LiveLinkHubClient->GetSubjectStaticData(InSubjectKey))
	{
		FLiveLinkStaticDataStruct StaticDataCopy;
		StaticDataCopy.InitializeWith(*StaticData);

		const FName OverridenName = GetSubjectNameOverride(InSubjectKey);
		LiveLinkProvider->UpdateSubjectStaticData(OverridenName, SubjectSettings->Role, MoveTemp(StaticDataCopy));
	}
}

void FLiveLinkHub::OnSubjectRemoved(FLiveLinkSubjectKey InSubjectKey) const
{
	// Send an update to connected clients as well.
	const FName OverridenName = GetSubjectNameOverride(InSubjectKey);
	LiveLinkProvider->RemoveSubject(OverridenName);
}

void FLiveLinkHub::BindCommands()
{
	const FLiveLinkHubCommands& Commands = FLiveLinkHubCommands::Get();
	CommandList->MapAction(Commands.NewConfig, FExecuteAction::CreateSP(this, &FLiveLinkHub::NewConfig));
	CommandList->MapAction(Commands.OpenConfig, FExecuteAction::CreateSP(this, &FLiveLinkHub::OpenConfig));
	CommandList->MapAction(Commands.SaveConfigAs, FExecuteAction::CreateSP(this, &FLiveLinkHub::SaveConfigAs));
	CommandList->MapAction(Commands.SaveConfig, FExecuteAction::CreateSP(this, &FLiveLinkHub::SaveConfig),
		FCanExecuteAction::CreateSP(this, &FLiveLinkHub::CanSaveConfig));
}

void FLiveLinkHub::NewConfig()
{
	SessionManager->NewSession();
}

void FLiveLinkHub::SaveConfigAs()
{
	SessionManager->SaveSessionAs();
}

bool FLiveLinkHub::CanSaveConfig() const
{
	return SessionManager->CanSaveCurrentSession();
}

void FLiveLinkHub::SaveConfig()
{
	SessionManager->SaveCurrentSession();
}

void FLiveLinkHub::OpenConfig()
{
	SessionManager->RestoreSession();
}

FName FLiveLinkHub::GetSubjectNameOverride(const FLiveLinkSubjectKey& InSubjectKey) const
{
	if (const TSharedPtr<ILiveLinkHubSessionManager> Manager = SessionManager)
	{
		if (const TSharedPtr<ILiveLinkHubSession> CurrentSession = Manager->GetCurrentSession())
		{
			if (TOptional<FLiveLinkHubSubjectProxy> SubjectProxy = CurrentSession->GetSubjectConfig(InSubjectKey))
			{
				return SubjectProxy->GetOutboundName();
			}
		}
	}

	return InSubjectKey.SubjectName;
}

#undef LOCTEXT_NAMESPACE /*LiveLinkHub*/
