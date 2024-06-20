// Copyright Epic Games, Inc. All Rights Reserved.
#include "IAudioInsightsModule.h"

#include "AudioInsightsModule.h"

IAudioInsightsTraceModule& IAudioInsightsModule::GetTraceModule()
{
	return GetChecked().GetTraceModule();
}

UE::Audio::Insights::FAudioInsightsModule& IAudioInsightsModule::GetChecked()
{
	using namespace UE::Audio::Insights;
	return static_cast<FAudioInsightsModule&>(FModuleManager::GetModuleChecked<IAudioInsightsModule>(FAudioInsightsModule::GetName()));
}
