// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHttpThreadedRequest.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "HttpManager.h"

void IHttpThreadedRequest::StartWaitingInQueue()
{
	TimeStartedWaitingInQueue = FPlatformTime::Seconds();
}

float IHttpThreadedRequest::GetTimeStartedWaitingInQueue() const
{
	check(TimeStartedWaitingInQueue != 0);
	return TimeStartedWaitingInQueue;
}
