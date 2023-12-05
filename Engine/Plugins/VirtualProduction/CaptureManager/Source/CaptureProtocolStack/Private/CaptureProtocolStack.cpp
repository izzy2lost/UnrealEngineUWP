// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureProtocolStack.h"

void FCaptureProtocolStackModule::StartupModule()
{
	TimerManager = MakeUnique<FCPSTimerManager>();
}

void FCaptureProtocolStackModule::ShutdownModule()
{
	TimerManager = nullptr;
}

FCPSTimerManager& FCaptureProtocolStackModule::GetTimerManager()
{
	return *TimerManager;
}

IMPLEMENT_MODULE(FCaptureProtocolStackModule, CaptureProtocolStack)
