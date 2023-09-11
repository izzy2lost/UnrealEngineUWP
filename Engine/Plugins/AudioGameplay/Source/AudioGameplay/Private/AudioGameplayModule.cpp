// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioGameplayModule.h"
#include "AudioGameplayLogs.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAudioGameplay"

DEFINE_LOG_CATEGORY(AudioGameplayLog);

void FAudioGameplayModule::StartupModule()
{
	UE_LOG(AudioGameplayLog, Log, TEXT("%s"), ANSI_TO_TCHAR(__FUNCTION__));
}

void FAudioGameplayModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAudioGameplayModule, AudioGameplay)
