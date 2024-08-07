// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubModule.h"

#include "Clients/LiveLinkHubProvider.h"
#include "Editor/EditorPerformanceSettings.h"
#include "LiveLinkHubApplication.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkHubSubjectSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Recording/LiveLinkHubRecordingController.h"
#include "Settings/LiveLinkHubSettings.h"
#include "Settings/LiveLinkHubSettingsCustomization.h"
#include "Subjects/LiveLinkHubSubjectSettingsDetailsCustomization.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "LiveLinkHubModule"

void FLiveLinkHubModule::PreinitializeLiveLinkHub()
{
	check(!LiveLinkHub);
	LiveLinkHub = MakeShared<FLiveLinkHub>();
	LiveLinkHub->Preinitialize();
}

void FLiveLinkHubModule::StartLiveLinkHub()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StartLiveLinkHub);
	checkf(LiveLinkHub, TEXT("Ensure PreinitializeLiveLinkHub was called first"));

	LiveLinkHub->Initialize();

	// Disable throttling for the hub
	GetMutableDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground = false;

#if IS_PROGRAM
	LiveLinkHubLoop(LiveLinkHub);
#endif
}

void FLiveLinkHubModule::ShutdownLiveLinkHub()
{
	LiveLinkHub.Reset();
}

void FLiveLinkHubModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(ULiveLinkHubSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkHubSettingsCustomization::MakeInstance));

	bUseSubjectSettingsDetailsCustomization =
		GConfig->GetBoolOrDefault(TEXT("LiveLink"), TEXT("bUseLiveLinkHubSubjectSettingsDetailsCustomization"), false, GEngineIni);
	if (bUseSubjectSettingsDetailsCustomization)
	{
		PropertyModule.RegisterCustomClassLayout(ULiveLinkHubSubjectSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLiveLinkHubSubjectSettingsDetailsCustomization::MakeInstance));
	}
}

void FLiveLinkHubModule::ShutdownModule()
{
	if (FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyEditorModule->UnregisterCustomClassLayout(ULiveLinkHubSettings::StaticClass()->GetFName());

		if (bUseSubjectSettingsDetailsCustomization)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ULiveLinkHubSubjectSettings::StaticClass()->GetFName());
		}
	}
}

TSharedPtr<FLiveLinkHub> FLiveLinkHubModule::GetLiveLinkHub() const
{
	return LiveLinkHub;
}

TSharedPtr<FLiveLinkHubProvider> FLiveLinkHubModule::GetLiveLinkProvider() const
{
	return LiveLinkHub ? LiveLinkHub->LiveLinkProvider : nullptr;
}

TSharedPtr<FLiveLinkHubRecordingController> FLiveLinkHubModule::GetRecordingController() const
{
	return LiveLinkHub ? LiveLinkHub->RecordingController : nullptr;
}

TSharedPtr<FLiveLinkHubRecordingListController> FLiveLinkHubModule::GetRecordingListController() const
{
	return LiveLinkHub ? LiveLinkHub->RecordingListController : nullptr;
}

TSharedPtr<FLiveLinkHubPlaybackController> FLiveLinkHubModule::GetPlaybackController() const
{
	return LiveLinkHub ? LiveLinkHub->PlaybackController : nullptr;
}

TSharedPtr<FLiveLinkHubSubjectController> FLiveLinkHubModule::GetSubjectController() const
{
	return LiveLinkHub ? LiveLinkHub->SubjectController : nullptr;
}

TSharedPtr<ILiveLinkHubSessionManager> FLiveLinkHubModule::GetSessionManager() const
{
	return LiveLinkHub ? LiveLinkHub->SessionManager : nullptr;
}

IMPLEMENT_MODULE(FLiveLinkHubModule, LiveLinkHub);
#undef LOCTEXT_NAMESPACE /* LiveLinkHubModule */
