// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoSourceGroup.h"

#include "EpicRtcVideoSource.h"
#include "Logging.h"
#include "PixelStreaming2PluginSettings.h"
#include "PixelStreaming2Trace.h"
#include "Stats.h"

namespace UE::PixelStreaming2
{
	TSharedPtr<FVideoSourceGroup> FVideoSourceGroup::Create(TSharedPtr<FVideoCapturer> InVideoCapturer)
	{
		TSharedPtr<FVideoSourceGroup> VideoSourceGroup = TSharedPtr<FVideoSourceGroup>(new FVideoSourceGroup());

		VideoSourceGroup->FrameDelegateHandle = InVideoCapturer->OnFrameCaptured.AddSP(VideoSourceGroup.ToSharedRef(), &FVideoSourceGroup::OnFrameCaptured);

		return VideoSourceGroup;
	}

	FVideoSourceGroup::FVideoSourceGroup()
		: bCoupleFramerate(!UPixelStreaming2PluginSettings::CVarDecoupleFramerate.GetValueOnAnyThread())
		, FramesPerSecond(UPixelStreaming2PluginSettings::CVarWebRTCFps.GetValueOnAnyThread())
	{
	}

	FVideoSourceGroup::~FVideoSourceGroup()
	{
		Stop();
	}

	void FVideoSourceGroup::SetFPS(int32 InFramesPerSecond)
	{
		FramesPerSecond = InFramesPerSecond;
	}

	int32 FVideoSourceGroup::GetFPS()
	{
		return FramesPerSecond;
	}

	void FVideoSourceGroup::SetCoupleFramerate(bool Couple)
	{
		bCoupleFramerate = Couple;
	}

	void FVideoSourceGroup::AddVideoSource(TSharedPtr<FEpicRtcVideoSource> VideoSource)
	{
		{
			FScopeLock Lock(&CriticalSection);
			VideoSources.Add(VideoSource);
		}

		CheckStartStopThread();
	}

	void FVideoSourceGroup::RemoveVideoSource(const FEpicRtcVideoSource* ToRemove)
	{
		{
			FScopeLock Lock(&CriticalSection);
			VideoSources.RemoveAll([ToRemove](const TSharedPtr<FEpicRtcVideoSource>& Target) {
				return Target.Get() == ToRemove;
			});
		}
		CheckStartStopThread();
	}

	void FVideoSourceGroup::RemoveAllVideoSources()
	{
		{
			FScopeLock Lock(&CriticalSection);
			VideoSources.Empty();
		}
		CheckStartStopThread();
	}

	void FVideoSourceGroup::Start()
	{
		if (!bRunning)
		{
			if (VideoSources.Num() > 0)
			{
				StartThread();
			}
			bRunning = true;
		}
	}

	void FVideoSourceGroup::Stop()
	{
		if (bRunning)
		{
			StopThread();
			bRunning = false;
		}
	}

	void FVideoSourceGroup::Tick()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL_STR("PixelStreaming2 Video Source Group Tick", PixelStreaming2Channel);
		FScopeLock Lock(&CriticalSection);

		// for each player session, push a frame
		for (auto& VideoSource : VideoSources)
		{
			if (VideoSource)
			{
				VideoSource->PushFrame();
			}
		}
	}

	void FVideoSourceGroup::OnFrameCaptured()
	{
		if (bCoupleFramerate)
		{
			Tick();
		}
		else
		{
			// We are in decoupled render/streaming mode - we should wake our VideoSourceGroup::FFrameThread (if it is sleeping)
			if (FrameRunnable && FrameRunnable->FrameEvent.Get())
			{
				FrameRunnable->FrameEvent.Get()->Trigger();
			}
		}
	}

	void FVideoSourceGroup::StartThread()
	{
		if (!bCoupleFramerate && !bThreadRunning)
		{
			FrameRunnable = MakeUnique<FFrameThread>(AsWeak());
			FrameThread = FRunnableThread::Create(FrameRunnable.Get(), TEXT("FVideoSourceGroup Thread"), 0, TPri_TimeCritical);
			bThreadRunning = true;
		}
	}

	void FVideoSourceGroup::StopThread()
	{
		if (FrameThread != nullptr)
		{
			FrameThread->Kill(true);
		}
		FrameThread = nullptr;
		bThreadRunning = false;
	}

	void FVideoSourceGroup::CheckStartStopThread()
	{
		if (bRunning)
		{
			const int32 NumSources = VideoSources.Num();
			if (bThreadRunning && NumSources == 0)
			{
				StopThread();
			}
			else if (!bThreadRunning && NumSources > 0)
			{
				StartThread();
			}
		}
	}

	bool FVideoSourceGroup::FFrameThread::Init()
	{
		return true;
	}

	uint32 FVideoSourceGroup::FFrameThread::Run()
	{
		bIsRunning = true;

		while (bIsRunning)
		{
			if (TSharedPtr<FVideoSourceGroup> VideoSourceGroup = OuterVideoSourceGroup.Pin())
			{
				const double TimeSinceLastSubmitMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - LastSubmitCycles);

				// Decrease this value to make expected frame delivery more precise, however may result in more old frames being sent
				const double PrecisionFactor = 0.1;
				const double WaitFactor = UPixelStreaming2PluginSettings::CVarDecoupleWaitFactor.GetValueOnAnyThread();

				// In "auto" mode vary this value based on historical average
				const double TargetSubmitMs = 1000.0 / VideoSourceGroup->FramesPerSecond;
				const double TargetSubmitMsWithPadding = TargetSubmitMs * WaitFactor;
				const double CloseEnoughMs = TargetSubmitMs * PrecisionFactor;
				const bool	 bFrameOverdue = TimeSinceLastSubmitMs >= TargetSubmitMsWithPadding;

				// Check frame arrived in time
				if (!bFrameOverdue)
				{
					// Frame arrived in a timely fashion, but is it too soon to maintain our target rate? If so, sleep.
					double WaitTimeRemainingMs = TargetSubmitMsWithPadding - TimeSinceLastSubmitMs;
					if (WaitTimeRemainingMs > CloseEnoughMs)
					{
						bool bGotNewFrame = FrameEvent.Get()->Wait(WaitTimeRemainingMs);
						if (!bGotNewFrame)
						{
							UE_LOG(LogPixelStreaming2, VeryVerbose, TEXT("Old frame submitted"));
						}
					}
				}

				// Push frame immediately
				PushFrame(VideoSourceGroup);
			}
		}
		return 0;
	}

	void FVideoSourceGroup::FFrameThread::Stop()
	{
		bIsRunning = false;
	}

	void FVideoSourceGroup::FFrameThread::Exit()
	{
		bIsRunning = false;
	}

	/*
	 * Note this function is required as part of `FSingleThreadRunnable` and only gets called when engine is run in single-threaded mode,
	 * so the logic is much less complex as this is not a case we particularly optimize for, a simple tick on an interval will be acceptable.
	 */
	void FVideoSourceGroup::FFrameThread::Tick()
	{
		if (TSharedPtr<FVideoSourceGroup> VideoSourceGroup = OuterVideoSourceGroup.Pin())
		{
			const uint64 NowCycles = FPlatformTime::Cycles64();
			const double DeltaMs = FPlatformTime::ToMilliseconds64(NowCycles - LastSubmitCycles);
			const double TargetSubmitMs = 1000.0 / VideoSourceGroup->FramesPerSecond;
			if (DeltaMs >= TargetSubmitMs)
			{
				PushFrame(VideoSourceGroup);
			}
		}
	}

	void FVideoSourceGroup::ForceKeyFrame()
	{
		TArray<TSharedPtr<FEpicRtcVideoSource>> VideoSourcesCopy;
		{
			// Grab a copy of the SharedPtr inside the lock to make sure it does not change elsewhere
			// while allowing the functions to be called on the video source outside the lock.
			FScopeLock Lock(&CriticalSection);
			VideoSourcesCopy = VideoSources;
		}

		for (auto& VideoSource : VideoSourcesCopy)
		{
			if (VideoSource)
			{
				VideoSource->ForceKeyFrame();
			}
		}
	}

	void FVideoSourceGroup::FFrameThread::PushFrame(TSharedPtr<FVideoSourceGroup> VideoSourceGroup)
	{
		VideoSourceGroup->Tick();
		LastSubmitCycles = FPlatformTime::Cycles64();
	}
} // namespace UE::PixelStreaming2
