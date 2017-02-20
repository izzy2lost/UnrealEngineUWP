// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "OnlineSubsystemLive.h"

void FOnlineAsyncTaskManagerLive::OnlineTick()
{
	check(LiveSubsystem);
	check(FPlatformTLS::GetCurrentThreadId() == OnlineThreadId);
}


