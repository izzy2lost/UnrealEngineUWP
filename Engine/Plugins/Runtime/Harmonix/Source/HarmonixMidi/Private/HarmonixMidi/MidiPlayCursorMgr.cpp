// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixMidi/MidiPlayCursorMgr.h"
#include "HarmonixMidi/MidiPlayCursor.h"

#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiReader.h"
#include "HarmonixMidi/MidiTrack.h"
#include "HarmonixMidi/BarMap.h"
#include "HarmonixMidi/TempoMap.h"

FMidiPlayCursorMgr::FMidiPlayCursorMgr()
	: SongMaps(&DefaultMaps)
	, LengthMs(0.f)
	, LengthTicks(0)
	, DirectMappedTimeFollower(false)
	, Loop(false)
	, LoopOffsetTick(0.0f)
	, LoopStartMs(0)
	, LoopStartTick(0)
	, LoopEndMs(0)
	, LoopEndTick(0)
	, MsSinceLowResUpdate(0.0f)
	, HiResLoopedSinceLastLoResUpdate(false)
	, InMidiChangeLock(false)
	, CurrentAdvanceRate(1.f)
{
	// setup the default tempo map to have one entry...
	DefaultMaps.GetTempoMap().AddTempoInfoPoint(MidiConstants::BPMToMidiTempo(120.0f), 0);
	DefaultMaps.GetBarMap().AddTimeSignatureAtBarIncludingCountIn(0, 4, 4);
}

void FMidiPlayCursorMgr::Reset()
{
	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);
	UnregisterAllPlayCursors();
	ResetTrackers();
	MidiFileData = nullptr;
	SongMaps = &DefaultMaps;
	LengthMs = 0.f;
	LengthTicks = 0;
	DirectMappedTimeFollower = false;
	Loop = false;
	LoopOffsetTick = 0.0f;
	LoopStartTick = 0;
	LoopStartMs = 0.0f;
	LoopEndTick = 0;
	LoopEndMs = 0.0f;
	MsSinceLowResUpdate = 0.0f;
	HiResLoopedSinceLastLoResUpdate = false;
	MidiFileData = nullptr;
	InMidiChangeLock = false;
}

void FMidiPlayCursorMgr::AttachToTimeAuthority(const TSharedPtr<FMidiPlayCursorMgr>& InTimeAuthority)
{
	TimeAuthority = InTimeAuthority;
}

void FMidiPlayCursorMgr::AttachToMidiResource(TSharedPtr<FMidiFileData> InMidiFileData, bool ResetTrackersToStart, int32 PreRollBars)
{
	LockForMidiDataChanges();
	MidiFileData = InMidiFileData;
	if (MidiFileData)
	{
		SongMaps = &MidiFileData->SongMaps;
	}
	else
	{
		SongMaps = &DefaultMaps;
	}
	if (ResetTrackersToStart)
	{
		ResetTrackers();
	}
	MidiDataChangeComplete(EMidiChangePositionCorrectMode::MaintainTick, PreRollBars);
}

void FMidiPlayCursorMgr::DetachFromMidiResource()
{
	LockForMidiDataChanges();
	MidiFileData = nullptr;
	SongMaps = &DefaultMaps;
	ResetTrackers();
	MidiDataChangeComplete(EMidiChangePositionCorrectMode::MaintainTick);
}

FMidiSongPos FMidiPlayCursorMgr::CalculateSongPosWithOffsetMs(float DeltaMs, bool IsLowRes) const
{
	FMidiSongPos OutSongPos;
	float Ms = (IsLowRes ? GetCurrentLowResMs() : GetCurrentHiResMs()) + DeltaMs;
	if (DoesLoop())
	{
		float LoopLengthMs = LoopEndMs - LoopStartMs;
		float MappedMs = LoopStartMs + FMath::Fmod(Ms - LoopStartMs, LoopLengthMs);
		OutSongPos.SetByTime(MappedMs, GetSongMaps());
	}
	else
	{
		OutSongPos.SetByTime(Ms, GetSongMaps());
	}

	// currently only use the time authority for the tempo
	if (TSharedPtr<FMidiPlayCursorMgr> TimeAuthorityPtr = TimeAuthority.Pin())
	{
		FMidiSongPos AuthoritySongPos = TimeAuthorityPtr->CalculateSongPosWithOffsetMs(DeltaMs, IsLowRes);

		OutSongPos.Tempo = AuthoritySongPos.Tempo;
	}

	return OutSongPos;
}

void FMidiPlayCursorMgr::DetermineLength()
{
	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);
	if (!MidiFileData)
	{
		LengthMs = 0.0f;
		LengthTicks = 0;
		LoopEndMs = 0.0f;
		LoopEndTick = 0;
		LoopStartMs = 0.0f;
		LoopStartTick = 0;
	}
	else
	{
		//Determine file length in ms and ticks
		LengthTicks = 0;
		const TArray<FMidiTrack>& Tracks = MidiFileData->Tracks;
		int32 NumTracks = Tracks.Num();
		for (int32 i = 0; i < NumTracks; ++i)
		{
			LengthTicks = FMath::Max(LengthTicks, Tracks[i].GetEvents().Last().GetTick());
		}
		//Round file length up to the nearest bar
		int32 Bar = FMath::CeilToInt32(SongMaps->GetBarMap().TickToFractionalBarIncludingCountIn(LengthTicks));
		LengthTicks = SongMaps->GetBarMap().BarIncludingCountInToTick(Bar);
		LengthMs = SongMaps->GetTempoMap().TickToMs(LengthTicks);

		LoopEndMs = LengthMs;
		LoopEndTick = LengthTicks;
		LoopStartMs = 0.0f;
		LoopStartTick = 0;
	}
}

FMidiPlayCursorMgr::~FMidiPlayCursorMgr()
{
	UnregisterAllPlayCursors();
}

void FMidiPlayCursorMgr::RegisterHiResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs)
{
	if (PlayCursor->Owner)
	{
		PlayCursor->Owner->UnregisterPlayCursor(PlayCursor);
	}
	PlayCursor->UnregisterASAP = false;
	FScopeLock HiResLock(&HiResCursorListLock);
	PlayCursor->SetOwner(this, &HiResTracker, PreRollMs);
	HiResTracker.AddCursor(PlayCursor);
}

void FMidiPlayCursorMgr::RegisterLowResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs)
{
	if (PlayCursor->Owner)
	{
		PlayCursor->Owner->UnregisterPlayCursor(PlayCursor);
	}
	PlayCursor->UnregisterASAP = false;
	FScopeLock LowResLock(&LowResCursorListLock);
	PlayCursor->SetOwner(this, &LowResTracker, PreRollMs);
	LowResTracker.AddCursor(PlayCursor);
}

void FMidiPlayCursorMgr::RecalculatePreRollDueToCursorPosition(FMidiPlayCursor* PlayCursor)
{
	float PreRollMs = 0.0f;
	if (PlayCursor->GetLookaheadType() == FMidiPlayCursor::ELookaheadType::Ticks)
	{
		PreRollMs = -GetTempoMap().TickToMs(-PlayCursor->GetLookaheadTicks());
	}
	else
	{
		PreRollMs = PlayCursor->GetLookaheadMs();
	}
	if (-PreRollMs < HiResTracker.CurrentMs)
	{
		// yup... earliest look ahead!
		int32 NewTick = GetTempoMap().MsToTick(-PreRollMs);

		// back up one tick...
		NewTick--;
		PreRollMs = GetTempoMap().TickToMs(NewTick);
		HiResTracker.Reset(NewTick, PreRollMs, false);
		LowResTracker.Reset(NewTick, PreRollMs, false);
	}
}

void FMidiPlayCursorMgr::UnregisterPlayCursor(FMidiPlayCursor* PlayCursor, bool WarnOnFail)
{
	bool WasHiRes = false;
	{
		FScopeLock HiResLock(&HiResCursorListLock);
		if (TraversingHiResCursors)
		{
			WasHiRes = HiResTracker.ContainsCursor(PlayCursor);
		}
		else
		{
			WasHiRes = HiResTracker.RemoveCursor(PlayCursor);
		}
		if (WasHiRes && TraversingHiResCursors)
		{
			UE_LOG(LogMidi, Error, TEXT("You cannot unregister a MidiPlayCursor while this manager is currently traversing it's cursor list!"));
			return;
		}
	}
	if (!WasHiRes)
	{
		// wasn't on hi-res list... try the low res...
		FScopeLock LowResLock(&LowResCursorListLock);
		bool WasLowRes = false;
		if (TraversingLowResCursors)
		{
			WasLowRes = LowResTracker.ContainsCursor(PlayCursor);
		}
		else
		{
			WasLowRes = LowResTracker.RemoveCursor(PlayCursor);
		}
		if (!WasLowRes)
		{
			if (WarnOnFail)
			{
				UE_LOG(LogMidi, Warning, TEXT("Attempt to remove MidiPlayCursor from Manager that didn't own it!"));
			}
			return;
		}
		if (WasLowRes && TraversingLowResCursors)
		{
			UE_LOG(LogMidi, Error, TEXT("You cannot unregister a MidiPlayCursor while this manager is currently traversing it's cursor list!"));
			return;
		}
	}
	// it was removed from one or the other lists...
	PlayCursor->SetOwner(nullptr, nullptr);
}

void FMidiPlayCursorMgr::UnregisterAllPlayCursors()
{
	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);

	if (TraversingHiResCursors || TraversingLowResCursors)
	{
		UE_LOG(LogMidi, Error, TEXT("You cannot unregister all MidiPlayCursors while this manager is currently traversing it's cursor list!"));
		return;
	}

	TraversingHiResCursors = true;
	for (auto it = HiResTracker.Cursors.begin(); it != HiResTracker.Cursors.end(); ++it)
	{
		it.GetNode()->ManagerIsDetaching();
		it.GetNode()->SetOwner(nullptr, nullptr);
	}
	HiResTracker.Clear();
	TraversingHiResCursors = false;
	TraversingLowResCursors = true;
	for (auto it = LowResTracker.Cursors.begin(); it != LowResTracker.Cursors.end(); ++it)
	{
		it.GetNode()->ManagerIsDetaching();
		it.GetNode()->SetOwner(nullptr, nullptr);
	}
	LowResTracker.Clear();
	TraversingLowResCursors = false;
}

bool FMidiPlayCursorMgr::HasLowResCursors() const
{
	return !LowResTracker.Cursors.IsEmpty();
}

void FMidiPlayCursorMgr::ResetTrackers()
{
	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);
	float Ms = GetTempoMap().TickToMs(-1);
	LowResTracker.Reset(-1, Ms, false);
	HiResTracker.Reset(-1, Ms, false);
	MsSinceLowResUpdate = 0.0f;
	HiResLoopedSinceLastLoResUpdate = false;
}

bool FMidiPlayCursorMgr::HasMidiFile() const
{
	return MidiFileData.IsValid();
}

int32 FMidiPlayCursorMgr::FindTrackIndexByName(const FString& name) const
{
	return MidiFileData.IsValid() ? MidiFileData->FindTrackIndexByName(name) : INDEX_NONE;
}

const FString* FMidiPlayCursorMgr::GetMidiFileName() const
{
	if (!MidiFileData)
	{
		return nullptr;
	}

	return &MidiFileData->MidiFileName;
}

const UMidiFile::FMidiTrackList& FMidiPlayCursorMgr::Tracks() const
{
	return MidiFileData.IsValid() ? MidiFileData->Tracks : DefaultTracks;
}

const FSongMaps& FMidiPlayCursorMgr::GetSongMaps() const
{
	check(SongMaps);
	return *SongMaps;
}

const FTempoMap& FMidiPlayCursorMgr::GetTempoMap() const
{
	check(SongMaps);
	return SongMaps->GetTempoMap();
}

const FBarMap& FMidiPlayCursorMgr::GetBarMap() const
{
	check(SongMaps);
	return SongMaps->GetBarMap();
}

void FMidiPlayCursorMgr::GetCursorExtentsMs(float& Earliest, float& Latest) const
{
	Earliest = (HiResTracker.EarliestCursorMs > LowResTracker.EarliestCursorMs) ?
		HiResTracker.EarliestCursorMs : LowResTracker.EarliestCursorMs;
	Latest = (HiResTracker.LatestCursorMs < LowResTracker.LatestCursorMs) ?
		HiResTracker.LatestCursorMs : LowResTracker.LatestCursorMs;
}

void FMidiPlayCursorMgr::GetCursorExtentsTicks(int32& Earliest, int32& Latest) const
{
	Earliest = (HiResTracker.EarliestCursorTick > LowResTracker.EarliestCursorTick) ?
		HiResTracker.EarliestCursorTick : LowResTracker.EarliestCursorTick;
	Latest = (HiResTracker.LatestCursorTick < LowResTracker.LatestCursorTick) ?
		HiResTracker.LatestCursorTick : LowResTracker.LatestCursorTick;
}

bool FMidiPlayCursorMgr::CursorsAllInPhase() const
{
	if (!Loop)
	{
		return true;
	}

	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);

	// Check tick driven...
	int32 EarliestTick, LatestTick;
	GetCursorExtentsTicks(EarliestTick, LatestTick);
	if ((HiResTracker.CurrentTick < LoopEndTick && HiResTracker.CurrentTick + EarliestTick > LoopEndTick) ||
		(LowResTracker.CurrentTick < LoopEndTick && LowResTracker.CurrentTick + EarliestTick > LoopEndTick) ||
		(HiResTracker.CurrentTick > LoopStartTick && HiResTracker.CurrentTick + LatestTick < LoopStartTick) ||
		(LowResTracker.CurrentTick > LoopStartTick && LowResTracker.CurrentTick + LatestTick < LoopStartTick))
	{
		return false;
	}

	// check time driven...
	float EarliestMs, LatestMs;
	GetCursorExtentsMs(EarliestMs, LatestMs);
	if ((HiResTracker.CurrentMs < LoopEndMs && HiResTracker.CurrentMs + EarliestMs > LoopEndMs) ||
		(LowResTracker.CurrentMs < LoopEndMs && LowResTracker.CurrentMs + EarliestMs > LoopEndMs) ||
		(HiResTracker.CurrentMs > LoopStartMs && HiResTracker.CurrentMs + LatestMs < LoopStartMs) ||
		(LowResTracker.CurrentMs > LoopStartMs && LowResTracker.CurrentMs + LatestMs < LoopStartMs))
	{
		return false;
	}

	return true;
}

int32 FMidiPlayCursorMgr::GetFarthestAheadCursorTick() const
{
	int32 Hrt = HiResTracker.GetFarthestAheadCursorTick();
	int32 Lrt = LowResTracker.GetFarthestAheadCursorTick();
	return((Lrt > Hrt) ? Lrt : Hrt);
}

int32 FMidiPlayCursorMgr::GetFarthestBehindCursorTick() const
{
	int32 Hrt = HiResTracker.GetFarthestBehindCursorTick();
	int32 Lrt = LowResTracker.GetFarthestBehindCursorTick();
	return((Lrt < Hrt) ? Lrt : Hrt);
}

float FMidiPlayCursorMgr::GetBufferedMs() const
{
	return (HiResTracker.EarliestCursorMs > LowResTracker.EarliestCursorMs) ? HiResTracker.EarliestCursorMs : LowResTracker.EarliestCursorMs;
}

void FMidiPlayCursorMgr::SetLoop(int32 StartTick, int32 EndTick, bool IsDirectMappedFollower, bool IgnoringLookAhead)
{
	// NOTE:
	// TODO: There is a missing test here that would check to see if the proposed loop end
	// might fall in the middle of the current span of leading and lagging play cursors,
	// which would result in ugly behavior!

	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);
	const FTempoMap& TempoMap = GetTempoMap();
	LoopStartTick = StartTick;
	LoopStartMs = TempoMap.TickToMs(StartTick);
	if (EndTick == kEndTick)
	{
		EndTick = LengthTicks;
	}
	LoopEndTick = EndTick;
	LoopEndMs = TempoMap.TickToMs(EndTick);
	Loop = true;
	DirectMappedTimeFollower = IsDirectMappedFollower;
	bool CursorsInPhase = CursorsAllInPhase();
	if (!CursorsInPhase)
	{
		if (IgnoringLookAhead)
		{
			HiResTracker.Reset(HiResTracker.CurrentTick, HiResTracker.CurrentMs, false);
			LowResTracker.Reset(LowResTracker.CurrentTick, LowResTracker.CurrentMs, false);
		}
		else
		{
			UE_LOG(LogMidi, Warning, TEXT("FMidiPlayCursorMgr::SetLoop : not ignoring look ahead, but cursors are not in phase!"));
		}
	}
}

void FMidiPlayCursorMgr::ClearLoop(bool IgnoringLookAhead)
{
	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);
	Loop = false;
	bool CursorsInPhase = CursorsAllInPhase();
	if (!CursorsInPhase)
	{
		if (IgnoringLookAhead)
		{
			HiResTracker.Reset(HiResTracker.CurrentTick, HiResTracker.CurrentMs, false);
			LowResTracker.Reset(LowResTracker.CurrentTick, LowResTracker.CurrentMs, false);
		}
		else
		{
			UE_LOG(LogMidi, Warning, TEXT("FMidiPlayCursorMgr::ClearLoop : not ignoring look ahead, but cursors are not in phase!"));
		}
	}
}

void FMidiPlayCursorMgr::SeekTo(int32 Tick, int32 PreRollBars, bool IsRenderThread, bool IsLoop)
{
	FScopeLock LowResLock(&LowResCursorListLock);
	FScopeLock HiResLock(&HiResCursorListLock);

	// Back up one tick, as we are going to be setting the cursors'
	// 'played through' position.
	Tick--;
	float NewPosMs = GetTempoMap().TickToMs(Tick);
	float PreRollStartMs = NewPosMs;
	int32 PreRollStartTick = Tick;
	if (PreRollBars > 0)
	{
		const FBarMap& Map = GetBarMap();
		float Bar = Map.TickToFractionalBarIncludingCountIn(Tick);
		Bar -= (float)PreRollBars;

		if (Bar < 0.0f)
		{
			PreRollStartTick = -1;
		}
		else
		{
			PreRollStartTick = Map.BarIncludingCountInToTick(Bar);
		}

		PreRollStartMs = GetTempoMap().TickToMs(PreRollStartTick);

		if (PreRollStartTick > Tick)
		{
			PreRollStartTick = Tick;
			PreRollStartMs = NewPosMs;
		}
	}

	if (IsLoop)
	{
		HiResLoopedSinceLastLoResUpdate = true;
	}

	// The hi-res cursors can just slam to the new position.
	// If this is resultOfDirectMappedLoop, the hi-res cursors
	// were already advanced to the end of the loop by the master.
	HiResTracker.Reset(Tick, NewPosMs, PreRollStartTick, PreRollStartMs, false);
	if (!IsRenderThread)
	{
		LowResTracker.Reset(Tick, NewPosMs, PreRollStartTick, PreRollStartMs, false);
	}
	else
	{
		LowResTracker.QueueReset(Tick, NewPosMs, PreRollStartTick, PreRollStartMs, false);
	}
}

void FMidiPlayCursorMgr::MoveToLoopStart()
{
	FScopeLock HiResLock(&HiResCursorListLock);
	{
		if (ensureMsgf(Loop, TEXT("That's odd. Asked to move to the beginning of the loop... but there is no loop!")))
		{
			const FTempoMap& TempoMap = GetTempoMap();

			int32   NewThruTick = LoopStartTick - 1;
			float NewThruMs = TempoMap.TickToMs(NewThruTick);

			// Move the hi-res cursor to the loop start
			HiResTracker.MoveToLoopStart(NewThruTick, NewThruMs);

			// The low res cursor will need to advance to the end of the loop, and then
			// advance from loop-start to current position at some point in the future
			// (during a low-res frame poll)
			MsSinceLowResUpdate += LoopEndMs - LowResTracker.CurrentMs;
			HiResLoopedSinceLastLoResUpdate = true;
		}
	}
}

bool FMidiPlayCursorMgr::IsDone() const
{
	if (Loop)
	{
		return false;
	}

	TraversingHiResCursors = true;
	// This next line of ugliness is required because const iterating through a const linked list is broken
	// and I don't have time to figure out what about the stack of templates is busted.
	TIntrusiveDoubleLinkedList<FMidiPlayCursor>& NonConstHiResCursorList = const_cast<TIntrusiveDoubleLinkedList<FMidiPlayCursor>&>(HiResTracker.Cursors);
	for (auto it = NonConstHiResCursorList.begin(); it != NonConstHiResCursorList.end(); it++)
	{
		if (!it.GetNode()->IsDone())
		{
			TraversingHiResCursors = false;
			return false;
		}
	}
	TraversingHiResCursors = false;
	TraversingLowResCursors = true;
	// This next line of ugliness is required because const iterating through a const linked list is broken
	// and I don't have time to figure out what about the stack of templates is busted.
	TIntrusiveDoubleLinkedList<FMidiPlayCursor>& NonConstLowResCursorList = const_cast<TIntrusiveDoubleLinkedList<FMidiPlayCursor>&>(LowResTracker.Cursors);
	for (auto it = NonConstLowResCursorList.begin(); it != NonConstLowResCursorList.end(); it++)
	{
		if (!it.GetNode()->IsDone())
		{
			TraversingLowResCursors = false;
			return false;
		}
	}
	TraversingLowResCursors = false;
	return true;
}

void FMidiPlayCursorMgr::LockForMidiDataChanges()
{
	if (ensureAlways(!InMidiChangeLock))
	{
		InMidiChangeLock = true;
		LowResCursorListLock.Lock();
		HiResCursorListLock.Lock();
	}
}

void FMidiPlayCursorMgr::MidiDataChangeComplete(EMidiChangePositionCorrectMode PositionMode, int32 PreRollBars)
{
	if (ensureAlways(InMidiChangeLock))
	{
		// We will need to recalculate extents. Unfortunately, this will also
		// blow away and previously set up loop points, so cache those...
		int32 OriginalLoopStartTick = LoopStartTick;
		int32 OriginalLoopEndTick = LoopEndTick;
		// Now fix up for possible new lengths...
		DetermineLength();
		// Now fix up looping...
		LoopStartTick = OriginalLoopStartTick;
		LoopEndTick = FMath::Min(LoopEndTick, OriginalLoopEndTick);
		LoopStartMs = GetTempoMap().TickToMs(LoopStartTick);
		LoopEndMs = GetTempoMap().TickToMs(LoopEndTick);

		if (PreRollBars == 0)
		{
			// Now fix up the hires cursors...
			TraversingHiResCursors = true;
			for (auto it = HiResTracker.Cursors.begin(); it != HiResTracker.Cursors.end(); ++it)
			{
				it.GetNode()->RecalcNextEventsDueToMidiChanges(PositionMode);
			}
			if (PositionMode == EMidiChangePositionCorrectMode::MaintainTick)
			{
				HiResTracker.CurrentMs = GetTempoMap().TickToMs(HiResTracker.CurrentTick);
			}
			else
			{
				HiResTracker.CurrentTick = GetTempoMap().MsToTick(HiResTracker.CurrentMs);
			}
			TraversingHiResCursors = false;
			// Now the low res cursors...
			TraversingLowResCursors = true;
			for (auto it = LowResTracker.Cursors.begin(); it != LowResTracker.Cursors.end(); ++it)
			{
				it.GetNode()->RecalcNextEventsDueToMidiChanges(PositionMode);
			}
			if (PositionMode == EMidiChangePositionCorrectMode::MaintainTick)
			{
				LowResTracker.CurrentMs = GetTempoMap().TickToMs(LowResTracker.CurrentTick);
			}
			else
			{
				LowResTracker.CurrentTick = GetTempoMap().MsToTick(LowResTracker.CurrentMs);
			}
			// Now deal with the possibility that the low res cursors might be "out of phase" with the hi res
			// cursors. This would happen if we are in the middle of a loop...
			if (HiResLoopedSinceLastLoResUpdate)
			{
				if (HiResTracker.CurrentMs < LowResTracker.CurrentMs)
				{
					MsSinceLowResUpdate = (LoopEndMs - LowResTracker.CurrentMs) + (HiResTracker.CurrentMs - LoopStartMs);
					check(MsSinceLowResUpdate >= 0.0f);
				}
				else
				{
					// yikes. how could this happen?
					checkNoEntry();
					HiResLoopedSinceLastLoResUpdate = false;
				}
			}
			TraversingLowResCursors = false;
		}
		else
		{
			int32 SeekTick = PositionMode == EMidiChangePositionCorrectMode::MaintainTick ? HiResTracker.CurrentTick : GetTempoMap().MsToTick(HiResTracker.CurrentMs);
			SeekTo(SeekTick, PreRollBars, true, false);
		}
		HiResCursorListLock.Unlock();
		LowResCursorListLock.Unlock();
		InMidiChangeLock = false;
	}
}

FMusicTimestamp FMidiPlayCursorMgr::GetMusicTimestampAtMs(float Ms) const
{
	const int32 TickAtOffset = GetSongMaps().MsToTick(Ms);
	return GetBarMap().TickToMusicTimestamp(TickAtOffset);
}

void FMidiPlayCursorMgr::AdvanceHiResToMs(float Ms, bool Broadcast)
{
	float DeltaMs = Ms - GetCurrentHiResMs();
	if (DeltaMs <= 0.0f)
	{
		return;
	}
	AdvanceHiResByDeltaMs(DeltaMs, Broadcast);
}

void FMidiPlayCursorMgr::AdvanceHiResByDeltaMs(float Ms, bool Broadcast)
{
	FScopeLock HiResLock(&HiResCursorListLock);
	HiResLoopedSinceLastLoResUpdate = AdvanceTrackerByDeltaMs(Ms, HiResTracker, Broadcast) || HiResLoopedSinceLastLoResUpdate;
	MsSinceLowResUpdate += Ms;
}

void FMidiPlayCursorMgr::AdvanceHiResByDeltaTick(int32 NumTicks, bool Broadcast)
{
	FScopeLock HiResLock(&HiResCursorListLock);

	int32 ThruTick = NumTicks + HiResTracker.CurrentTick;

	float StartMs = HiResTracker.CurrentMs;
	AdvanceTrackerThruTick(ThruTick, HiResTracker, false, Broadcast);
	MsSinceLowResUpdate += HiResTracker.CurrentMs - StartMs;
}

void FMidiPlayCursorMgr::AdvanceHiResThruTick(int32 ThruTick, bool Broadcast, bool DontAdvancePastLoopEnd)
{
	FScopeLock HiResLock(&HiResCursorListLock);

	// early out?...
	if (ThruTick == HiResTracker.CurrentTick)
	{
		return;
	}

	if (Loop && DontAdvancePastLoopEnd && ThruTick >= LoopEndTick - 1)
	{
		ThruTick = LoopEndTick - 1;
	}

	if (ThruTick < HiResTracker.CurrentTick)
	{
		UE_LOG(LogMidi, Warning, TEXT("Asked to go back in time without a seek!"));
		return;
	}

	float StartMs = HiResTracker.CurrentMs;
	AdvanceTrackerThruTick(ThruTick, HiResTracker, false, Broadcast);
	MsSinceLowResUpdate += HiResTracker.CurrentMs - StartMs;
}

bool FMidiPlayCursorMgr::AdvanceLowResCursors()
{
	FScopeLock LowResLock(&LowResCursorListLock);

	LowResTracker.HandleQueuedReset();

	float MsDiff = 0.0f;
	bool  HiResLooped = false;
	float HiResCurrentMs = 0.0f;
	{
		FScopeLock HiResLock(&HiResCursorListLock);

		MsDiff = MsSinceLowResUpdate;
		HiResCurrentMs = HiResTracker.CurrentMs;
		HiResLooped = HiResLoopedSinceLastLoResUpdate;
		MsSinceLowResUpdate = 0.0f;
		HiResLoopedSinceLastLoResUpdate = false;
	}

	if (!HiResLooped)
	{
		MsDiff = HiResCurrentMs - LowResTracker.CurrentMs;
		if (MsDiff <= 0.0f)
		{
			UpdateLowResCursors(LowResTracker);
			return false;
		}
	}
	return AdvanceTrackerByDeltaMs(MsDiff, LowResTracker, true, true);
}

void FMidiPlayCursorMgr::UpdateLowResCursors(FMidiPlayCursorTracker& Tracker)
{
	TraversingLowResCursors = true;
	for (auto it = Tracker.Cursors.begin(); it != Tracker.Cursors.end();)
	{
		if (!it.GetNode()->UpdateWithTrackerUnchanged())
		{
			auto DeadIt = it;
			++it;
			Tracker.Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
	TraversingLowResCursors = false;
}

bool FMidiPlayCursorMgr::AdvanceTrackerByDeltaMs(float Ms, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast)
{
	Tracker.ElapsedMs += Ms;
	const FTempoMap& TempoMap = GetTempoMap();
	float NewMs = Tracker.CurrentMs + Ms;
	int32 NewTick = (int32)(TempoMap.MsToTick(NewMs) + 0.5f);
	bool  Looped = false;

	if (NewTick >= LoopEndTick && Tracker.CurrentTick < LoopEndTick && Loop) // && !mDirectMappedFollower) If this is a direct mapped follower, than this function only gets called for the lo-res cursor, which needs to do normal loop handling!
	{
		// loop the tick around...
		NewTick = (NewTick - LoopEndTick) + LoopStartTick;
		NewMs = TempoMap.TickToMs(NewTick);
		Tracker.CurrentMs = NewMs;
		Tracker.CurrentTick = NewTick;
		Looped = true;
	}
	else
	{
		Tracker.CurrentTick = NewTick;
		Tracker.CurrentMs = NewMs;
	}

	bool* TraversingFlag = IsLowRes ? &TraversingLowResCursors : &TraversingHiResCursors;
	*TraversingFlag = true;
	for (auto it = Tracker.Cursors.begin(); it != Tracker.Cursors.end();)
	{
		if (!it.GetNode()->Advance(IsLowRes))
		{
			auto DeadIt = it;
			++it;
			Tracker.Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
	*TraversingFlag = false;

	return Looped;
}

void FMidiPlayCursorMgr::AdvanceTrackerThruTick(int32 ToTick, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast)
{
	const FTempoMap& TempoMap = GetTempoMap();

	float NewMs = TempoMap.TickToMs(ToTick);
	Tracker.ElapsedMs += NewMs - Tracker.CurrentMs;
	int32   NewTick = ToTick;

	Tracker.CurrentTick = NewTick;
	Tracker.CurrentMs = NewMs;

	bool* TraversingFlag = IsLowRes ? &TraversingLowResCursors : &TraversingHiResCursors;
	*TraversingFlag = true;
	for (auto it = Tracker.Cursors.begin(); it != Tracker.Cursors.end();)
	{
		if (!it.GetNode()->Advance(IsLowRes))
		{
			auto DeadIt = it;
			++it;
			Tracker.Cursors.Remove(DeadIt.GetNode());
			DeadIt.GetNode()->SetOwner(nullptr, nullptr);
		}
		else
		{
			++it;
		}
	}
	*TraversingFlag = false;
}
