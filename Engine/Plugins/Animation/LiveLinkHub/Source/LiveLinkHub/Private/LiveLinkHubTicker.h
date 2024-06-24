// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "LiveLinkHub.h"
#include "Misc/Timespan.h"
#include "Settings/LiveLinkHubSettings.h"

/** Object used to tick LiveLinkHub outside of the game thread. */
class FLiveLinkHubTicker : public FRunnable
{
public:
	explicit FLiveLinkHubTicker(TSharedRef<FLiveLinkHub> InLiveLinkHub)
		: LiveLinkHub(MoveTemp(InLiveLinkHub))
	{
	}

	void StartTick()
	{
		if (!bIsRunning)
		{
			bIsRunning = true;
			TickEvent = FGenericPlatformProcess::GetSynchEventFromPool();
			check(TickEvent);
			Thread.Reset(FRunnableThread::Create(this, TEXT("LiveLinkHubTicker")));
		}
	}

	//~ Begin FRunnable Interface
	virtual uint32 Run() override
	{
		float TickFrequency = 1 / GetDefault<ULiveLinkHubSettings>()->TargetFrameRate;
		FTimespan TickTimeSpan = FTimespan::FromSeconds(TickFrequency);

		while (bIsRunning)
		{
			check(TickEvent);
			TickEvent->Wait(TickTimeSpan);

			if (bIsRunning) // make sure we were not told to exit during the wait
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FLiveLinkHubTicker::Tick);
				LiveLinkHub->Tick();
			}
		}

		return 0;
	}
	virtual void Exit() override
	{
		if (!bIsRunning)
		{
			return;
		}

		bIsRunning = false;

		if (TickEvent)
		{
			TickEvent->Trigger();
		}

		Thread->WaitForCompletion();

		if (TickEvent)
		{
			FGenericPlatformProcess::ReturnSynchEventToPool(TickEvent);
			TickEvent = nullptr;
		}
	}
	//~ End FRunnable Interface
	std::atomic<bool> bIsRunning = false;

	FEvent* TickEvent = nullptr;
	TUniquePtr<FRunnableThread> Thread;
	TSharedPtr<FLiveLinkHub> LiveLinkHub;
};
