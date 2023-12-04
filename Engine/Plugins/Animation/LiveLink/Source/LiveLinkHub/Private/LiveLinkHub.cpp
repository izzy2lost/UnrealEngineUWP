// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHub.h"

#include "Clients/LiveLinkHubClientsController.h"
#include "Clients/LiveLinkHubProvider.h"
#include "Config/LiveLinkHubConfigData.h"
#include "Config/LiveLinkHubFileUtilities.h"
#include "DesktopPlatformModule.h"
#include "EditorDirectories.h"
#include "Features/IModularFeatures.h"
#include "Framework/Application/SlateApplication.h"
#include "IDesktopPlatform.h"
#include "LiveLinkHubClient.h"
#include "LiveLinkHubCommands.h"
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
	CommandList = MakeShared<FUICommandList>();

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

void FLiveLinkHub::BindCommands()
{
	const FLiveLinkHubCommands& Commands = FLiveLinkHubCommands::Get();
	CommandList->MapAction(Commands.NewConfig, FExecuteAction::CreateSP(this, &FLiveLinkHub::NewConfig));
	CommandList->MapAction(Commands.OpenConfig, FExecuteAction::CreateSP(this, &FLiveLinkHub::OpenConfig));
	CommandList->MapAction(Commands.SaveConfigAs, FExecuteAction::CreateSP(this, &FLiveLinkHub::SaveConfigAs));
	CommandList->MapAction(Commands.SaveConfig, FExecuteAction::CreateSP(this, &FLiveLinkHub::SaveConfig),
		FCanExecuteAction::CreateSP(this, &FLiveLinkHub::CanSaveConfig));
}

void FLiveLinkHub::ClearClient()
{
	check(LiveLinkProvider.IsValid());
	check(LiveLinkHubClient.IsValid());
	
	TMap<FMessageAddress, FLiveLinkHubUEClientInfo> Clients = LiveLinkProvider->GetClientsMap();
	
	for (const TTuple<FMessageAddress, FLiveLinkHubUEClientInfo>& Client : Clients)
	{
		LiveLinkProvider->RemoveClient(Client.Key);
	}
	
	LiveLinkHubClient->RemoveAllSources();

	// Removing sources only sets bPendingKill to true, need to tick to make sure they are removed.
	LiveLinkHubClient->ForceTick();
}

void FLiveLinkHub::NewConfig()
{
	ClearClient();
	LastConfigPath.Empty();
}

void FLiveLinkHub::SaveConfigAs()
{
	const FString FileDescription = UE::LiveLinkHub::FileUtilities::Private::ConfigDescription;
	const FString Extensions = UE::LiveLinkHub::FileUtilities::Private::ConfigExtension;
	const FString FileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *FileDescription, *Extensions, *Extensions);

	const FString DefaultFile = UE::LiveLinkHub::FileUtilities::Private::ConfigDefaultFileName;
	
	TArray<FString> SaveFileNames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	
	const bool bFileSelected = DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		LOCTEXT("LiveLinkHubSaveAsTitle", "Save As").ToString(),
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_SAVE),
		DefaultFile,
		FileTypes,
		EFileDialogFlags::None,
		SaveFileNames);

	if (bFileSelected && SaveFileNames.Num() > 0)
	{
		LastConfigPath = SaveFileNames[0];
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_SAVE, FPaths::GetPath(LastConfigPath));
		SaveConfig();
	}
}

bool FLiveLinkHub::CanSaveConfig() const
{
	return !LastConfigPath.IsEmpty();
}

void FLiveLinkHub::SaveConfig()
{
	if (LastConfigPath.IsEmpty())
	{
		return;
	}
	
	check(LiveLinkProvider.IsValid())
	check(LiveLinkHubClient.IsValid());
	
	FLiveLinkHubConfigData LiveLinkHubConfigData;
	
	TArray<FGuid> SourceGuids = LiveLinkHubClient->GetSources();
	for (const FGuid& SourceGuid : SourceGuids)
	{
		LiveLinkHubConfigData.Sources.Add(LiveLinkHubClient->GetSourcePreset(SourceGuid, nullptr));
	}

	TArray<FLiveLinkSubjectKey> Subjects = LiveLinkHubClient->GetSubjects(true, true);
	for (const FLiveLinkSubjectKey& Subject : Subjects)
	{
		LiveLinkHubConfigData.Subjects.Add(LiveLinkHubClient->GetSubjectPreset(Subject, nullptr));
	}

	const TMap<FMessageAddress, FLiveLinkHubUEClientInfo>& ClientMap = LiveLinkProvider->GetClientsMap();
	
	for (const TTuple<FMessageAddress, FLiveLinkHubUEClientInfo>& ClientKeyVal : ClientMap)
	{
		LiveLinkHubConfigData.Clients.Add(ClientKeyVal.Value);
	}

	UE::LiveLinkHub::FileUtilities::Private::SaveConfig(LiveLinkHubConfigData, LastConfigPath);
}

void FLiveLinkHub::OpenConfig()
{
	const FString FileDescription = UE::LiveLinkHub::FileUtilities::Private::ConfigDescription;
	const FString Extensions = UE::LiveLinkHub::FileUtilities::Private::ConfigExtension;
	const FString FileTypes = FString::Printf(TEXT("%s (*.%s)|*.%s"), *FileDescription, *Extensions, *Extensions);

	const FString DefaultFile = UE::LiveLinkHub::FileUtilities::Private::ConfigDefaultFileName;

	check(LiveLinkProvider.IsValid())
	check(LiveLinkHubClient.IsValid());
	
	ClearClient();
	
	TArray<FString> OpenFileNames;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	const bool bFileSelected = DesktopPlatform->OpenFileDialog(
		ParentWindowWindowHandle,
		LOCTEXT("LiveLinkHubOpenTitle", "Open").ToString(),
		FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN),
		DefaultFile,
		FileTypes,
		EFileDialogFlags::None,
		OpenFileNames);

	if (bFileSelected && OpenFileNames.Num() > 0)
	{
		LastConfigPath = OpenFileNames[0];
		FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_OPEN, FPaths::GetPath(LastConfigPath));

		const TSharedPtr<FLiveLinkHubConfigData> ConfigData = UE::LiveLinkHub::FileUtilities::Private::LoadConfig(LastConfigPath);
		if (ConfigData.IsValid())
		{
			for (const FLiveLinkSourcePreset& SourcePreset : ConfigData->Sources)
			{
				LiveLinkHubClient->CreateSource(SourcePreset);
			}

			for (const FLiveLinkSubjectPreset& SubjectPreset : ConfigData->Subjects)
			{
				LiveLinkHubClient->CreateSubject(SubjectPreset);	
			}

			for (const FLiveLinkHubUEClientInfo& Client : ConfigData->Clients)
			{
				LiveLinkProvider->AddClient(Client, FMessageAddress::NewAddress());
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE /*LiveLinkHub*/
