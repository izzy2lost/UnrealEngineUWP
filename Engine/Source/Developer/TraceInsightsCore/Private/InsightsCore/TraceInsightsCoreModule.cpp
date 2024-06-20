// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceInsightsCoreModule.h"

#include "Modules/ModuleManager.h"

#include "InsightsCore/Common/InsightsCoreStyle.h"

IMPLEMENT_MODULE(FTraceInsightsCoreModule, TraceInsightsCore);

void FTraceInsightsCoreModule::StartupModule()
{
	UE::Insights::FInsightsCoreStyle::Initialize();
}

void FTraceInsightsCoreModule::ShutdownModule()
{
	UE::Insights::FInsightsCoreStyle::Shutdown();
}
