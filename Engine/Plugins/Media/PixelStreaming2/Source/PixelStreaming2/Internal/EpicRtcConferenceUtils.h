// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EpicRtcTickableTask.h"

#include "epic_rtc/core/conference.h"

namespace UE::PixelStreaming2
{

	class PIXELSTREAMING2_API FEpicRtcTickConferenceTask : public FEpicRtcTickableTask
	{
	public:
		FEpicRtcTickConferenceTask(TRefCountPtr<EpicRtcConferenceInterface> EpicRtcConference, const FString& TaskName = TEXT("EpicRtcTickConferenceTask"))
			: EpicRtcConference(EpicRtcConference)
			, TaskName(TaskName)
		{
		}

		virtual ~FEpicRtcTickConferenceTask() override
		{
			// We may get a call to destroy the task before we've had a chance to tick again.
			// So to be safe, we tick the conference a final time
			if (EpicRtcConference)
			{

				while (EpicRtcConference->NeedsTick())
				{
					EpicRtcConference->Tick();
				}
			}
		}

		/** Begin FEpicRtcTickableTask */
		virtual void Tick(float DeltaMs) override
		{
			if (EpicRtcConference)
			{
				MsSinceLastAudioTick += DeltaMs;

				// Tick conference normally. This handles things like data channel message
				while (EpicRtcConference->NeedsTick())
				{
					EpicRtcConference->Tick();
				}

				// Tick audio (every 10 ms). This enables the pulling of audio from the ADM
				if (MsSinceLastAudioTick >= 10.f)
				{
					EpicRtcConference->TickAudio();
					MsSinceLastAudioTick = 0.f;
				}
			}
		}

		virtual const FString& GetName() const override
		{
			return TaskName;
		}
		/** End FEpicRtcTickableTask */

	private:
		TRefCountPtr<EpicRtcConferenceInterface> EpicRtcConference;
		FString									 TaskName;

		float MsSinceLastAudioTick = 0.f;
	};

} // namespace UE::PixelStreaming2