// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/SingleThreadRunnable.h"
#include "VideoCapturer.h"
#include "Templates/SharedPointer.h"

namespace UE::PixelStreaming2
{
	class FEpicRtcVideoSource;

	class FVideoSourceGroup : public TSharedFromThis<FVideoSourceGroup>
	{
	public:
		static TSharedPtr<FVideoSourceGroup> Create(TSharedPtr<FVideoCapturer> InVideoCapturer);
		~FVideoSourceGroup();

		void  SetFPS(int32 InFramesPerSecond);
		int32 GetFPS();

		void SetCoupleFramerate(bool Couple);

		void AddVideoSource(TSharedPtr<FEpicRtcVideoSource> VideoSource);
		void RemoveVideoSource(const FEpicRtcVideoSource* ToRemove);
		void RemoveAllVideoSources();

		void Start();
		void Stop();
		void Tick();
		bool IsThreadRunning() const { return bRunning; }

		void ForceKeyFrame();

	private:
		FVideoSourceGroup();

		void StartThread();
		void StopThread();
		void CheckStartStopThread();
		void OnFrameCaptured();

		class FFrameThread : public FRunnable, public FSingleThreadRunnable
		{
		public:
			FFrameThread(TWeakPtr<FVideoSourceGroup> InVideoSourceGroup)
				: OuterVideoSourceGroup(InVideoSourceGroup)
			{
			}
			virtual ~FFrameThread() = default;

			virtual bool   Init() override;
			virtual uint32 Run() override;
			virtual void   Stop() override;
			virtual void   Exit() override;

			virtual FSingleThreadRunnable* GetSingleThreadInterface() override
			{
				bIsRunning = true;
				return this;
			}

			virtual void Tick() override;

			void PushFrame(TSharedPtr<FVideoSourceGroup> VideoSourceGroup);

			bool						bIsRunning = false;
			TWeakPtr<FVideoSourceGroup> OuterVideoSourceGroup = nullptr;
			uint64						LastSubmitCycles = 0;

			/* Use this event to signal when we should wake and also how long we should sleep for between transmitting a frame. */
			FEventRef FrameEvent;
		};

		bool									bRunning = false;
		bool									bThreadRunning = false;
		bool									bCoupleFramerate = false;
		int32									FramesPerSecond = 30;
		TUniquePtr<FFrameThread>				FrameRunnable;
		FRunnableThread*						FrameThread = nullptr; // constant FPS tick thread
		TArray<TSharedPtr<FEpicRtcVideoSource>> VideoSources;

		FDelegateHandle FrameDelegateHandle;

		mutable FCriticalSection CriticalSection;
	};
} // namespace UE::PixelStreaming2
