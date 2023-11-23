// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubPlaybackController.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "ILiveLinkClient.h"
#include "Implementations/LiveLinkUAssetRecordingPlayer.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkRecording.h"
#include "LiveLinkPreset.h"
#include "LiveLinkTypes.h"
#include "Features/IModularFeatures.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

FLiveLinkHubPlaybackController::FLiveLinkHubPlaybackController()
{
	Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	
	RecordingPlayer = MakeUnique<FLiveLinkUAssetRecordingPlayer>();
}

void FLiveLinkHubPlaybackController::StartPlayback()
{
	PlaybackStartTime = FPlatformTime::Seconds();
	Playhead = 0;
	bIsInPlayback = true;
	
	FPlatformMisc::MemoryBarrier();
	PlaybackEvent->Trigger();
}

void FLiveLinkHubPlaybackController::PlayRecording(ULiveLinkRecording* InLiveLinkRecording)
{
	PreparePlayback(InLiveLinkRecording);
}

void FLiveLinkHubPlaybackController::StopPlayback()
{
	bIsInPlayback = false;
}

void FLiveLinkHubPlaybackController::PreparePlayback(ULiveLinkRecording* InLiveLinkRecording)
{
	if (InLiveLinkRecording)
	{
		if (bIsInPlayback)
		{
			check(RecordingToPlay);
			UE_LOG(LogLiveLinkHub, Warning, TEXT("Can't start playback since we are already doing playback"));
			return;
		}

		RecordingToPlay.Reset(InLiveLinkRecording);

		RollbackPreset.Reset(NewObject<ULiveLinkPreset>(GetTransientPackage(), TEXT("RecordingRollbackPreset")));
		// Save the current state of the sources/subjects in a rollback preset.
		RollbackPreset->BuildFromClient();

		RecordingPlayer->PreparePlayback(RecordingToPlay.Get());

		RecordingToPlay->RecordingPreset->ApplyToClientLatent(
			[this](bool)
			{
				StartPlayback();
			});
	}
	else
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Started a recording playback with an invalid recording."));
	}
}

FLiveLinkHubPlaybackController::~FLiveLinkHubPlaybackController()
{
	StopPlayback();
	Stopping = true;
	PlaybackEvent->Trigger();

	if (Thread.IsValid())
	{
		Thread->WaitForCompletion();
		Thread.Reset();
	}
}

void FLiveLinkHubPlaybackController::Start()
{
	FString ThreadName = TEXT("LiveLinkHub Playback Controller ");
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());

	Thread.Reset(FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask()));
}

void FLiveLinkHubPlaybackController::Stop()
{
	Stopping = true;
}

uint32 FLiveLinkHubPlaybackController::Run()
{
	while (!Stopping.load())
	{
		PlaybackEvent->Wait();

		while (bIsInPlayback)
		{
			if (Playhead >= RecordingToPlay->LengthInSeconds)
			{
				if (bLoopPlayback && RecordingToPlay->LengthInSeconds != 0)
				{
					Playhead = 0.0;
					PlaybackStartTime = FPlatformTime::Seconds();

					RecordingPlayer->RestartPlayback();
				}
				else
				{
					// Stop playback
					break;
				}
			}
			
			TArray<FLiveLinkRecordedFrame> NextFrames = RecordingPlayer->FetchNextFrames(Playhead);

			for (const FLiveLinkRecordedFrame& NextFrame : NextFrames)
			{
				// If we're sending static data
				if (NextFrame.LiveLinkRole)
				{
					FLiveLinkStaticDataStruct StaticDataStruct;
					StaticDataStruct.InitializeWith(NextFrame.Data.GetScriptStruct(), (FLiveLinkBaseStaticData*)NextFrame.Data.GetMemory());
					Client->PushSubjectStaticData_AnyThread(NextFrame.SubjectKey, NextFrame.LiveLinkRole, MoveTemp(StaticDataStruct));
				}
				else
				{
					FLiveLinkFrameDataStruct FrameDataStruct;
					FrameDataStruct.InitializeWith(NextFrame.Data.GetScriptStruct(), (FLiveLinkBaseFrameData*)NextFrame.Data.GetMemory());

					Client->PushSubjectFrameData_AnyThread(NextFrame.SubjectKey, MoveTemp(FrameDataStruct));
				}
			}

			Playhead = FPlatformTime::Seconds() - PlaybackStartTime;

			// Don't sleep if we pushed frames since that can take a small amount of time.
			if (!NextFrames.Num())
			{
				FPlatformProcess::Sleep(0.002);
				Playhead = FPlatformTime::Seconds() - PlaybackStartTime;
			}
		}

		// If the loop ended because the recording is over.
		bIsInPlayback = false;

		// Recording is done, clear the pointer.
		CurrentRecording.Reset();
		
		// Trigger the playback finished delegate on the game thread.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FLiveLinkHubPlaybackController::OnPlaybackFinished_Internal), TStatId(), nullptr, ENamedThreads::GameThread);
	}

	return 0;
}

void FLiveLinkHubPlaybackController::OnPlaybackFinished_Internal()
{
	RollbackPreset->ApplyToClientLatent();
	RecordingToPlay.Reset();

	PlaybackFinishedDelegate.Broadcast();
}
