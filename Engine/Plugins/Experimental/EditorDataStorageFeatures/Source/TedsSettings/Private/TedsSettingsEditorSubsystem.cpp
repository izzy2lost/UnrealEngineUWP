// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsSettingsEditorSubsystem.h"

#include "Editor.h"
#include "TedsSettingsLog.h"
#include "TedsSettingsManager.h"

namespace UE::EditorDataStorage::Settings
{
	static TAutoConsoleVariable<bool> CVarTedsSettingsEnable(
		TEXT("TEDS.Feature.Settings.Enable"),
		false,
		TEXT("When true, settings objects from the ISettingsModule will be mirrored to rows in the editor data storage."));
}

UTedsSettingsEditorSubsystem::UTedsSettingsEditorSubsystem()
	: UEditorSubsystem()
	, SettingsManager{ MakeShared<FTedsSettingsManager>() }
	, EnabledChangedDelegate{ }
{
}

const bool UTedsSettingsEditorSubsystem::IsEnabled() const
{
	return UE::EditorDataStorage::Settings::CVarTedsSettingsEnable.GetValueOnGameThread();
}

UTedsSettingsEditorSubsystem::FOnEnabledChanged& UTedsSettingsEditorSubsystem::OnEnabledChanged()
{
	return EnabledChangedDelegate;
}

void UTedsSettingsEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTedsSettings, Log, TEXT("UTedsSettingsEditorSubsystem::Initialize"));

	UE::EditorDataStorage::Settings::CVarTedsSettingsEnable->SetOnChangedCallback(
		FConsoleVariableDelegate::CreateLambda([this](IConsoleVariable* Variable)
		{
			const bool bIsEnabled = Variable->GetBool();
			
			if (bIsEnabled)
			{
				SettingsManager->Initialize();
			}
			else
			{
				SettingsManager->Shutdown();
			}

			EnabledChangedDelegate.Broadcast();
		}));

	if (UE::EditorDataStorage::Settings::CVarTedsSettingsEnable.GetValueOnGameThread())
	{
		SettingsManager->Initialize();
	}
}

void UTedsSettingsEditorSubsystem::Deinitialize()
{
	UE_LOG(LogTedsSettings, Log, TEXT("UTedsSettingsEditorSubsystem::Deinitialize"));

	if (UE::EditorDataStorage::Settings::CVarTedsSettingsEnable.GetValueOnGameThread())
	{
		SettingsManager->Shutdown();
	}

	Super::Deinitialize();
}
