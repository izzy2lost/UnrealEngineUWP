// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundMusicClockDriver.h"
#include "MetasoundGeneratorHandle.h"
#include "Components/AudioComponent.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundGenerator.h"
#include "Engine/World.h"
#include "Harmonix.h"

bool FMetasoundMusicClockDriver::CalculateSongPosWithOffset(float MsOffset, ECalibratedMusicTimebase Timebase, FMidiSongPos& OutResult) const
{
	// if we have an owner, ask them directly
	if (const FMidiPlayCursorMgr* Owner = Cursor.GetOwner())
	{
		float RawMs = Owner->GetCurrentHiResMs();
		switch (Timebase)
		{
		case ECalibratedMusicTimebase::AudioRenderTime:
			OutResult = Owner->CalculateSongPosWithOffsetMs((Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset - RawMs);
			break;
		case ECalibratedMusicTimebase::ExperiencedTime:
			OutResult = Owner->CalculateSongPosWithOffsetMs((Clock->CurrentPlayerExperiencedSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset - RawMs);
			break;
		case ECalibratedMusicTimebase::VideoRenderTime:
		default:
			OutResult = Owner->CalculateSongPosWithOffsetMs((Clock->CurrentVideoRenderSongPos.SecondsIncludingCountIn * 1000.0f) + MsOffset - RawMs);
			break;
		}
		return true;
	}
	return false;
}

bool FMetasoundMusicClockDriver::RefreshCurrentSongPos()
{
	//	Only for use when on the game thread.
	if (ensureMsgf(
			IsInGameThread(),
			TEXT("%hs called from non-game thread.  This is not supported"),
			__FUNCTION__) == false)
	{
		return false;
	}

	if (AudioComponentToWatch.IsValid())
	{
		if (!CurrentGeneratorHandle)
		{
			AttemptToConnectToAudioComponentsMetasound();
		}
		else if (!Cursor.GetOwner())
		{
			TryToRegisterPlayCursor();
		}
	}

	if (Cursor.GetOwner())
	{
		// cursor is attached and has the current info
		RefreshCurrentSongPosFromCursor();
		return true;
	}
	else
	{
		// Cursor not attached so use wall clock
		if (!WasEverConnected || Clock->RunPastMusicEnd)
		{
			RefreshCurrentSongPosFromWallClock();
			return true;
		}
	}
	return false;
}

void FMetasoundMusicClockDriver::OnStart()
{
	SongPosOffsetMs = 0.0f;
	FreeRunStartTimeSecs = Clock ? Clock->GetWorld()->GetTimeSeconds() : 0.0;
}

void FMetasoundMusicClockDriver::OnContinue()
{
	if (!Cursor.GetOwner())
	{
		RefreshCurrentSongPosFromWallClock();
	}
}

void FMetasoundMusicClockDriver::Disconnect()
{
	if (FMidiPlayCursorMgr* OldOwner = Cursor.GetOwner())
	{
		OldOwner->UnregisterPlayCursor(&Cursor);
	}
	DetachAllCallbacks();
	AudioComponentToWatch.Reset();
	CurrentGeneratorHandle.Reset();
}

const FSongMaps* FMetasoundMusicClockDriver::GetCurrentSongMaps() const
{
	if (Cursor.GetOwner())
	{
		return &Cursor.GetOwner()->GetSongMaps();
	}
	return &Clock->DefaultMaps;
}

bool FMetasoundMusicClockDriver::ConnectToAudioComponentsMetasound(UAudioComponent* InAudioComponent, FName MetasoundOuputPinName)
{
	AudioComponentToWatch = InAudioComponent;
	MetasoundOutputName = MetasoundOuputPinName;
	return AttemptToConnectToAudioComponentsMetasound();
}

bool FMetasoundMusicClockDriver::AttemptToConnectToAudioComponentsMetasound()
{
	if (!AudioComponentToWatch.IsValid() || MetasoundOutputName.IsNone())
	{
		return false;
	}

	DetachAllCallbacks();

	CurrentGeneratorHandle.Reset(UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(AudioComponentToWatch.Get()));
	if (!CurrentGeneratorHandle)
	{
		return false;
	}
	GeneratorAttachedCallbackHandle = CurrentGeneratorHandle->OnGeneratorHandleAttached.AddLambda([this](){OnGeneratorAttached();});
	GeneratorDetachedCallbackHandle = CurrentGeneratorHandle->OnGeneratorHandleDetached.AddLambda([this](){OnGeneratorDetached();});
	UMetasoundGeneratorHandle::FOnSetGraph::FDelegate OnSetGraph;
	OnSetGraph.BindLambda([this](){OnGraphSet();});
	GraphChangedCallbackHandle = CurrentGeneratorHandle->AddGraphSetCallback(MoveTemp(OnSetGraph));
	OnGeneratorAttached();
	return true;
}

void FMetasoundMusicClockDriver::DetachAllCallbacks()
{
	if (CurrentGeneratorHandle)
	{
		CurrentGeneratorHandle->OnGeneratorHandleAttached.Remove(GeneratorAttachedCallbackHandle);
		GeneratorAttachedCallbackHandle.Reset();
		CurrentGeneratorHandle->OnGeneratorHandleDetached.Remove(GeneratorDetachedCallbackHandle);
		GeneratorDetachedCallbackHandle.Reset();
		CurrentGeneratorHandle->RemoveGraphSetCallback(GraphChangedCallbackHandle);
		GraphChangedCallbackHandle.Reset();
	}
}

void FMetasoundMusicClockDriver::OnGeneratorAttached()
{
	TryToRegisterPlayCursor();
}

void FMetasoundMusicClockDriver::OnGraphSet()
{
	TryToRegisterPlayCursor();
}

void FMetasoundMusicClockDriver::OnGeneratorDetached()
{
	check(Clock);
	check(IsInGameThread());
	if (FMidiPlayCursorMgr* OldOwner = Cursor.GetOwner())
	{
		OldOwner->UnregisterPlayCursor(&Cursor);
		if (Clock->GetState() != EMusicClockState::Stopped)
		{
			Clock->DefaultMaps.Copy(OldOwner->GetSongMaps(), 0, Cursor.GetCurrentTick());
			SongPosOffsetMs = Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f;
			FreeRunStartTimeSecs = Clock->GetWorld()->GetTimeSeconds();
		}
		Clock->MusicClockDisconnectedEvent.Broadcast();
	}
}

void FMetasoundMusicClockDriver::TryToRegisterPlayCursor()
{
	check(Clock);
	check(IsInGameThread());
	if (FMidiPlayCursorMgr* OldOwner = Cursor.GetOwner())
	{
		OldOwner->UnregisterPlayCursor(&Cursor);
	}
	if (CurrentGeneratorHandle && !MetasoundOutputName.IsNone())
	{
		TSharedPtr<Metasound::FMetasoundGenerator> LowLevelGenerator = CurrentGeneratorHandle->GetGenerator();
		if (LowLevelGenerator.IsValid())
		{
			const TOptional<Metasound::TDataReadReference<HarmonixMetasound::FMidiClock>> MidiClockRef = LowLevelGenerator->GetOutputReadReference<HarmonixMetasound::FMidiClock>(MetasoundOutputName);
			const Metasound::TDataReadReference<HarmonixMetasound::FMidiClock>* MidiClock = MidiClockRef.GetPtrOrNull();
			if (MidiClock)
			{
				(*MidiClock)->RegisterLowResPlayCursor(&Cursor);
				WasEverConnected = true;
				Clock->MusicClockConnectedEvent.Broadcast();
			}
			else
			{
				UE_LOG(LogMusicClock, Verbose, TEXT("Didn't find Midi Clock output named \"%s\" in the Metasound!"), *MetasoundOutputName.ToString());
			}
		}
	}
}

void FMetasoundMusicClockDriver::RefreshCurrentSongPosFromWallClock()
{
	check(Clock);

	bool TempoChanged = Clock->CurrentSmoothedAudioRenderSongPos.Tempo != Clock->Tempo;

	double FreeRunTime = (Clock->GetWorld()->GetTimeSeconds() - FreeRunStartTimeSecs) * Clock->CurrentClockAdvanceRate;

	Clock->CurrentSmoothedAudioRenderSongPos.SetByTime(((float)FreeRunTime * 1000.0) + SongPosOffsetMs, Clock->DefaultMaps);
	Clock->CurrentPlayerExperiencedSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs(), Clock->DefaultMaps);
	Clock->CurrentVideoRenderSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredVideoToAudioRenderOffsetMs(), Clock->DefaultMaps);
	if (Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn > Clock->RawUnsmoothedAudioRenderPos.SecondsIncludingCountIn)
	{
		Clock->RawUnsmoothedAudioRenderPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f, Clock->DefaultMaps);
	}

	if (TempoChanged)
	{
		Clock->Tempo = Clock->CurrentSmoothedAudioRenderSongPos.Tempo;
		Clock->CurrentBeatDurationSec = (60.0f / Clock->Tempo) / Clock->CurrentClockAdvanceRate;
		Clock->CurrentBarDurationSec = ((Clock->TimeSignatureNum * Clock->CurrentBeatDurationSec) / (Clock->TimeSignatureDenom / 4.0f)) / Clock->CurrentClockAdvanceRate;
	}
}

void FMetasoundMusicClockDriver::RefreshCurrentSongPosFromCursor()
{
	check(Clock);
	Clock->CurrentSmoothedAudioRenderSongPos = Cursor.GetCurrentSongPos();

	float PrevClockAdvanceRate = Clock->CurrentClockAdvanceRate;
	if (const FMidiPlayCursorMgr* Owner = Cursor.GetOwner())
	{
		Clock->CurrentClockAdvanceRate = Owner->GetCurrentAdvanceRate();
		float RawMs = Owner->GetCurrentHiResMs();
		Clock->CurrentPlayerExperiencedSongPos = Owner->CalculateSongPosWithOffsetMs(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs() - RawMs);
		Clock->CurrentVideoRenderSongPos = Owner->CalculateSongPosWithOffsetMs(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::GetMeasuredVideoToAudioRenderOffsetMs() - RawMs);
		Clock->RawUnsmoothedAudioRenderPos = Owner->CalculateSongPosWithOffsetMs(0.0f);
	}
	else
	{
		Clock->CurrentPlayerExperiencedSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::Get().GetMeasuredUserExperienceAndReactionToAudioRenderOffsetMs(), Clock->DefaultMaps);
		Clock->CurrentVideoRenderSongPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f - FHarmonixModule::Get().GetMeasuredVideoToAudioRenderOffsetMs(), Clock->DefaultMaps);
		if (Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn > Clock->RawUnsmoothedAudioRenderPos.SecondsIncludingCountIn)
		{
			Clock->RawUnsmoothedAudioRenderPos.SetByTime(Clock->CurrentSmoothedAudioRenderSongPos.SecondsIncludingCountIn * 1000.0f, Clock->DefaultMaps);
		}
	}

	Clock->TimeSignatureNum = Clock->CurrentSmoothedAudioRenderSongPos.TimeSigNumerator;
	Clock->TimeSignatureDenom = Clock->CurrentSmoothedAudioRenderSongPos.TimeSigDenominator;

	if (Clock->Tempo != Clock->CurrentSmoothedAudioRenderSongPos.Tempo || PrevClockAdvanceRate != Clock->CurrentClockAdvanceRate)
	{
		Clock->Tempo = Clock->CurrentSmoothedAudioRenderSongPos.Tempo;
		Clock->CurrentBeatDurationSec = (60.0f / Clock->Tempo) / Clock->CurrentClockAdvanceRate;
		Clock->CurrentBarDurationSec = ((Clock->TimeSignatureNum * Clock->CurrentBeatDurationSec) / (Clock->TimeSignatureDenom / 4.0f)) / Clock->CurrentClockAdvanceRate;
	}
}
