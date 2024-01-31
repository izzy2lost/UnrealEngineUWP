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
#include "UI/Widgets/SLiveLinkHubPlaybackWidget.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

FLiveLinkHubPlaybackController::FLiveLinkHubPlaybackController()
{
	Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	
	RecordingPlayer = MakeUnique<FLiveLinkUAssetRecordingPlayer>();
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

TSharedRef<SWidget> FLiveLinkHubPlaybackController::MakePlaybackWidget()
{
	return SNew(SLiveLinkHubPlaybackWidget)
		.IsEnabled_Raw(this, &FLiveLinkHubPlaybackController::IsReady)
		.OnPlayForward_Raw(this, &FLiveLinkHubPlaybackController::BeginPlayback, false)
		.OnPlayReverse_Raw(this, &FLiveLinkHubPlaybackController::BeginPlayback, true)
		.OnFirstFrame_Lambda([this]()
		{
			GoToFrame(GetSelectionStartFrame());
		})
		.OnLastFrame_Lambda([this]()
		{
			GoToFrame(GetSelectionEndFrame());
		})
		.OnPreviousFrame_Lambda([this]()
		{
			GoToFrame(CurrentFrameIndex - 1);
		})
		.OnNextFrame_Lambda([this]()
		{
			GoToFrame(CurrentFrameIndex + 1);
		})
		.SetCurrentTime_Raw(this, &FLiveLinkHubPlaybackController::GoToTime)
		.GetViewRange_Lambda([this]()
		{
			return SliderViewRange;
		})
		.SetViewRange_Lambda([this](TRange<double> NewRange)
		{
			SliderViewRange = MoveTemp(NewRange);
		})
		.GetTotalLength_Raw(this, &FLiveLinkHubPlaybackController::GetLength)
		.GetCurrentTime_Raw(this, &FLiveLinkHubPlaybackController::GetCurrentTime)
		.GetCurrentFrame_Raw(this, &FLiveLinkHubPlaybackController::GetCurrentFrame)
		.GetSelectionStartTime_Raw(this, &FLiveLinkHubPlaybackController::GetSelectionStartTime)
		.SetSelectionStartTime_Raw(this,& FLiveLinkHubPlaybackController::SetSelectionStartTime)
		.GetSelectionEndTime_Raw(this, &FLiveLinkHubPlaybackController::GetSelectionEndTime)
		.SetSelectionEndTime_Raw(this,& FLiveLinkHubPlaybackController::SetSelectionEndTime)
		.IsPaused_Raw(this, &FLiveLinkHubPlaybackController::IsPaused)
		.IsInReverse_Raw(this, &FLiveLinkHubPlaybackController::IsPlayingInReverse)
		.IsLooping_Raw(this, &FLiveLinkHubPlaybackController::IsLooping)
		.OnSetLooping_Raw(this, &FLiveLinkHubPlaybackController::SetLooping)
		.GetTimeDelta_Raw(this, &FLiveLinkHubPlaybackController::GetTimeDelta);
}

void FLiveLinkHubPlaybackController::StartPlayback()
{
	ResumePlayback();
	PlaybackStartTime = FPlatformTime::Seconds();
	
	FPlatformMisc::MemoryBarrier();
	PlaybackEvent->Trigger();
}

void FLiveLinkHubPlaybackController::ResumePlayback()
{
	bIsInPlayback = true;
	bIsPaused = false;
	StartTimestamp = GetTimeFromFrameIndex(GetCurrentFrame());
	// Force sync so interpolation doesn't interfere if the first frame isn't the current frame
	SyncToFrame(CurrentFrameIndex);
}

void FLiveLinkHubPlaybackController::PreparePlayback(ULiveLinkRecording* InLiveLinkRecording)
{
	if (InLiveLinkRecording == nullptr)
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Started a recording playback with an invalid recording."));
	}
	else if (InLiveLinkRecording != RecordingToPlay.Get())
	{
		if (RecordingToPlay.IsValid())
		{
			Eject();
		}
		
		RecordingToPlay.Reset(InLiveLinkRecording);

		// The start and end of playback.
		SelectionStartTime = 0.f;
		SelectionEndTime = GetLength();
		
		// The range the user sees.
		SliderViewRange = TRange<double>(SelectionStartTime, SelectionEndTime);
		
		RollbackPreset.Reset(NewObject<ULiveLinkPreset>(GetTransientPackage(), TEXT("RecordingRollbackPreset")));
		// Save the current state of the sources/subjects in a rollback preset.
		RollbackPreset->BuildFromClient();

		RecordingPlayer->PreparePlayback(RecordingToPlay.Get());

		RecordingToPlay->RecordingPreset->ApplyToClientLatent([this](bool)
		{
			bIsReady = true;
			SyncToFrame(0); // Needed to establish connection with client
		});
	}
}

void FLiveLinkHubPlaybackController::PlayRecording(ULiveLinkRecording* InLiveLinkRecording)
{
	PreparePlayback(InLiveLinkRecording);
}

void FLiveLinkHubPlaybackController::BeginPlayback(bool bInReverse)
{
	const bool bReverseChange = bIsReverse != bInReverse;
	bIsReverse = bInReverse;
	
	// Either we are paused and should unpause, or we are toggling forward/reverse play modes.
	if (bIsPaused || !bIsInPlayback || bReverseChange)
	{
		if (ShouldRestart())
		{
			// Check if we're at the end of the recording and restart, ie user pressed play again.
			RestartPlayback();
		}
		else
		{
			if (bIsReverse)
			{
				RecordingPlayer->RestartPlayback(CurrentFrameIndex);
			}
			
			// Resume as normal for anywhere else in the recording.
			PlaybackStartTime = FPlatformTime::Seconds();
		}
		
		ResumePlayback();
	}
	else if (bIsInPlayback)
	{
		PausePlayback();
	}

	FPlatformMisc::MemoryBarrier();
	PlaybackEvent->Trigger();
}

void FLiveLinkHubPlaybackController::RestartPlayback()
{
	CurrentFrameIndex = INDEX_NONE;

	const bool bOldReverse = bIsReverse; // Stop playback resets reverse
	StopPlayback();
	StartTimestamp = GetTimeFromFrameIndex(CurrentFrameIndex);
	RecordingPlayer->RestartPlayback(CurrentFrameIndex);
	bIsInPlayback = true;
	bIsReverse = bOldReverse;
}

void FLiveLinkHubPlaybackController::PausePlayback()
{
	bIsPaused = true;
}

void FLiveLinkHubPlaybackController::StopPlayback()
{
	bIsInPlayback = false;

	// Wait for the playback thread to exit...
	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
	if (CurrentThreadId != Thread->GetThreadID())
	{
		while(!bIsPlaybackWaiting) { }
	}
	
	const bool bReverse = bIsReverse.load();
	Playhead = bReverse ? GetSelectionEndTime() : GetSelectionStartTime();
	if (CurrentFrameIndex == INDEX_NONE)
	{
		CurrentFrameIndex = GetFrameIndexFromTime(bReverse ? GetSelectionEndTime() : GetSelectionStartTime());
	}
	PlaybackStartTime = FPlatformTime::Seconds();

	RecordingPlayer->RestartPlayback();
	bIsReverse = false;
}

void FLiveLinkHubPlaybackController::Eject()
{
	bIsReady = false;
	
	StopPlayback();
	
	bIsPaused = false;
	CurrentFrameIndex = 0;
	RecordingPlayer->RestartPlayback(CurrentFrameIndex);

	SetSelectionStartTime(0.f);
	SetSelectionEndTime(0.f);
	Playhead = 0.f;
	StartTimestamp = 0.f;
	
	if (RollbackPreset.IsValid())
	{
		RollbackPreset->ApplyToClientLatent();
	}
	// Recording is done, clear the pointer.
	RecordingToPlay.Reset();
}

void FLiveLinkHubPlaybackController::GoToTime(double InTime)
{
	// Stop needs to occur to restart playback.
	StopPlayback();

	PlaybackStartTime -= InTime;
	Playhead = InTime;

	const int32 FrameIndex = GetFrameIndexFromTime(InTime);
	SyncToFrame(FrameIndex);
}

void FLiveLinkHubPlaybackController::GoToFrame(int32 InFrameIndex)
{
	// Stop needs to occur to restart playback.
	StopPlayback();

	const double NewTime = GetTimeFromFrameIndex(InFrameIndex);
	PlaybackStartTime -= NewTime;
	Playhead = NewTime;
	
	SyncToFrame(InFrameIndex);
}

int32 FLiveLinkHubPlaybackController::GetFrameIndexFromTime(double InTime, bool bReverse) const
{
	return RecordingPlayer->PlayheadToFrameIndex(InTime, bReverse);
}

double FLiveLinkHubPlaybackController::GetTimeFromFrameIndex(int32 InFrameIndex) const
{
	return RecordingPlayer->FrameIndexToPlayhead(InFrameIndex);
}

int32 FLiveLinkHubPlaybackController::GetSelectionStartFrame() const
{
	return GetFrameIndexFromTime(GetSelectionStartTime(), bIsReverse);
}

int32 FLiveLinkHubPlaybackController::GetSelectionEndFrame() const
{
	return GetFrameIndexFromTime(GetSelectionEndTime(), bIsReverse);
}

double FLiveLinkHubPlaybackController::GetSelectionStartTime() const
{
	return SelectionStartTime;
}

void FLiveLinkHubPlaybackController::SetSelectionStartTime(double InTime)
{
	SelectionStartTime = InTime;
}

double FLiveLinkHubPlaybackController::GetSelectionEndTime() const
{
	return SelectionEndTime;
}

void FLiveLinkHubPlaybackController::SetSelectionEndTime(double InTime)
{
	SelectionEndTime = InTime;
}

double FLiveLinkHubPlaybackController::GetLength() const
{
	return RecordingToPlay ? RecordingToPlay->LengthInSeconds : 0.f;
}

double FLiveLinkHubPlaybackController::GetCurrentTime() const
{
	return Playhead.load();
}

int32 FLiveLinkHubPlaybackController::GetCurrentFrame() const
{
	return CurrentFrameIndex;
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
		bIsPlaybackWaiting = true;
		PlaybackEvent->Wait();
		bIsPlaybackWaiting = false;
		while (bIsInPlayback)
		{
			if (bIsPaused)
			{
				FPlatformProcess::Sleep(0.002);
			}
			else
			{
				const bool bSynced = SyncToPlayhead();

				auto SetPlayhead = [&]()
				{
					const double Delta = FPlatformTime::Seconds() - PlaybackStartTime;
					Playhead = bIsReverse ? StartTimestamp - Delta : StartTimestamp + Delta;
					Playhead = FMath::Clamp(Playhead.load(), GetSelectionStartTime(), GetSelectionEndTime());
				};

				SetPlayhead();
			
				// Don't sleep if we pushed frames since that can take a small amount of time.
				if (!bSynced)
				{
					FPlatformProcess::Sleep(0.002);
					SetPlayhead();
				}
			}
			
			if (ShouldRestart())
			{
				if (bLoopPlayback && RecordingToPlay->LengthInSeconds != 0 && !bIsPaused)
				{
					RestartPlayback();
				}
				else
				{
					// Stop playback
					break;
				}
			}

		}

		// If the loop ended because the recording is over.
		bIsInPlayback = false;
		
		// Trigger the playback finished delegate on the game thread.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FLiveLinkHubPlaybackController::OnPlaybackFinished_Internal), TStatId(), nullptr, ENamedThreads::GameThread);
	}

	return 0;
}

void FLiveLinkHubPlaybackController::OnPlaybackFinished_Internal()
{
	PlaybackFinishedDelegate.Broadcast();
}

void FLiveLinkHubPlaybackController::PushSubjectData(const FLiveLinkRecordedFrame& NextFrame, bool bForceSync)
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
		// Record the frame index when pushing so it is accurate. If we only calculate based on time it may not match the actual frames sent.
		CurrentFrameIndex = NextFrame.FrameIndex;
		
		FLiveLinkFrameDataStruct FrameDataStruct;
		FrameDataStruct.InitializeWith(NextFrame.Data.GetScriptStruct(), (FLiveLinkBaseFrameData*)NextFrame.Data.GetMemory());
		
		if (bForceSync)
		{
			FrameDataStruct.GetBaseData()->MetaData.StringMetaData.Add(TEXT("ForceSync"), TEXT("true"));
		}
		Client->PushSubjectFrameData_AnyThread(NextFrame.SubjectKey, MoveTemp(FrameDataStruct));
	}
}

bool FLiveLinkHubPlaybackController::SyncToPlayhead()
{
	const double Timestamp = Playhead.load();
	TArray<FLiveLinkRecordedFrame> NextFrames = bIsReverse ? RecordingPlayer->FetchPreviousFramesAtTimestamp(Timestamp)
		: RecordingPlayer->FetchNextFramesAtTimestamp(Timestamp);
	
	for (const FLiveLinkRecordedFrame& NextFrame : NextFrames)
	{
		// todo: Have to forcesync -- interpolation fails due to improper frame times
		const bool bForceSync = bIsReverse;
		PushSubjectData(NextFrame, bForceSync);
	}
	
	return NextFrames.Num() > 0;
}

bool FLiveLinkHubPlaybackController::SyncToFrame(int32 InFrameIndex)
{
	TArray<FLiveLinkRecordedFrame> NextFrames = RecordingPlayer->FetchNextFramesAtIndex(InFrameIndex);
	
	for (const FLiveLinkRecordedFrame& NextFrame : NextFrames)
	{
		PushSubjectData(NextFrame, true);
	}

	return NextFrames.Num() > 0;
}

bool FLiveLinkHubPlaybackController::ShouldRestart() const
{
	return RecordingToPlay.IsValid() && ((bIsReverse && CurrentFrameIndex <= GetSelectionStartFrame()) || (!bIsReverse && CurrentFrameIndex >= GetSelectionEndFrame()));
}

double FLiveLinkHubPlaybackController::GetTimeDelta() const
{
	// todo: get frame frate
	return RecordingToPlay.IsValid() ? RecordingToPlay->LengthInSeconds / 60.f : 1.f;
}
