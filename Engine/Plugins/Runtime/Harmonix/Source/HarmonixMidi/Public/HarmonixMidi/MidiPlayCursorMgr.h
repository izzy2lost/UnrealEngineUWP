// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/MidiPlayCursorTracker.h"
#include "HarmonixMidi/SongMaps.h"
#include "HarmonixMidi/MidiSongPos.h"

class FMidiPlayCursor;

class HARMONIXMIDI_API FMidiPlayCursorMgr
{
public:
	FMidiPlayCursorMgr();
	virtual ~FMidiPlayCursorMgr();

	void Reset();

	void AttachToTimeAuthority(const TSharedPtr<FMidiPlayCursorMgr>& InTimeAuthority);
	void DetachFromTimeAuthority() { TimeAuthority = nullptr; }

	void AttachToMidiResource(TSharedPtr<FMidiFileData> MidiDataProxy, bool ResetTrackersToStart = true, int32 PreRollBars = 0);
	void DetachFromMidiResource();

	FMidiSongPos CalculateSongPosWithOffsetMs(float Ms) const;

	//////////////////////////////////////////////////////////////////////////
	// Play cursor management
	//////////////////////////////////////////////////////////////////////////
	enum class ESetupOptions
	{
		NoBroadcastNoPreRoll,
		BroadcastImmediately,
		PreRollIfAtStartOrBroadcast,
		PreRollIfAtStartOrNoBroadcast
	};
	void  RegisterHiResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs = -1.0f);
	void  RegisterLowResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs = -1.0f);
	void  UnregisterPlayCursor(FMidiPlayCursor* PlayCursor, bool WarnOnFail = true);
	void  UnregisterAllPlayCursors();
	bool  HasLowResCursors() const;
	void  RecalculatePreRollDueToCursorPosition(FMidiPlayCursor* PlayCursor);
	void  GetCursorExtentsMs(float& Earliest, float& Latest) const;
	void  GetCursorExtentsTicks(int32& Earliest, int32& Latest) const;
	bool  CursorsAllInPhase() const;
	int32 GetFarthestAheadCursorTick() const;
	int32 GetFarthestBehindCursorTick() const;
	float GetBufferedMs() const;

	// Do we loop when we hit the end of the file?
	// Will function incorrectly if you change this after passing the end
	static const int32 kEndTick = -1;
	void SetLoop(int32 StartTick, int32 EndTick, bool IsDirectMappedFollower, bool IgnoringLookAhead);
	void ClearLoop(bool IgnoringLookAhead);

	void SeekTo(int32 Tick, int32 PreRollBars, bool IsRenderThread, bool IsLoop);
	void MoveToLoopStart();

	// NOTE: This next function DOES NOT affect how quickly the play cursor manager advances!
	// The manager is advanced as desired with calls to the Advance___ functions. This simply 
	// sets a member to inform the manager and the manager's cursors how quickly the driver of
	// this manager is advancing time as this might be useful information for some cursors
	// (eg. smoothing cursors)
	void  InformOfCurrentAdvanceRate(float Rate) { CurrentAdvanceRate = Rate; }
	float GetCurrentAdvanceRate() const { return CurrentAdvanceRate; }

	//If broadcast is true, call callbacks for each midi event that we pass
	//If broadcast is false, silently update the internals of each play cursor
	void AdvanceHiResToMs(float Ms, bool Broadcast);
	void AdvanceHiResByDeltaMs(float Ms, bool Broadcast);
	void AdvanceHiResThruTick(int32 ThruTick, bool Broadcast, bool DontAdvancePastLoopEnd = false);
	void AdvanceHiResByDeltaTick(int32 NumTicks, bool Broadcast);

	// returns whether the cursors looped
	bool AdvanceLowResCursors();

	void ResetTrackers(); //reset to the beginning of the song

	float GetLoopStartMs() const { return LoopStartMs; }
	int32 GetLoopStartTick() const { return LoopStartTick; }
	float GetLoopEndMs() const { return LoopEndMs; }
	int32 GetLoopEndTick() const { return LoopEndTick; }
	bool  DoesLoop() const { return Loop; }
	bool  IsDirectMappedTimeFollower() const { return DirectMappedTimeFollower; }
	int32 GetLengthTicks() const { return LengthTicks; }

	bool   HasMidiFile() const;
	int32  FindTrackIndexByName(const FString& name) const;
	const FString* GetMidiFileName() const;

	const UMidiFile::FMidiTrackList& Tracks() const;

	const FSongMaps& GetSongMaps() const;
	const FTempoMap& GetTempoMap() const;
	const FBarMap& GetBarMap() const;

	int32 GetCurrentHiResTick() const   { return HiResTracker.CurrentTick;  }
	float GetCurrentHiResMs() const     { return HiResTracker.CurrentMs;    }
	float GetElapsedHiResMs() const     { return HiResTracker.ElapsedMs;    }
	int32 GetCurrentLowResTick() const  { return LowResTracker.CurrentTick; }
	float GetCurrentLowResMs() const    { return LowResTracker.CurrentMs;   }
	float GetElapsedLowResMs() const    { return LowResTracker.ElapsedMs;   }

	bool IsDone() const;

	void LockForMidiDataChanges();

	enum class EMidiChangePositionCorrectMode
	{
		MaintainTick,
		MaintainTime
	};

	void MidiDataChangeComplete(EMidiChangePositionCorrectMode positionMode, int32 PreRollBars = 0);
	
	/**
	 * @brief Get the music timestamp at an absolute time in milliseconds
	 * @param Ms - The time in milliseconds
	 * @return The music timestamp
	 */
	FMusicTimestamp GetMusicTimestampAtMs(float Ms) const;

protected:

	friend class FMidiPlayCursor;

	// REFACTOR NEEDED... the are too many places that need to know if a
	// given tracker is hires or lowres. So the tracker needs to know which it is!
	FMidiPlayCursorTracker HiResTracker;
	FMidiPlayCursorTracker LowResTracker;

private:
	bool AdvanceTrackerByDeltaMs(float Ms, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast = true);
	void AdvanceTrackerThruTick(int32 ToTick, FMidiPlayCursorTracker& Tracker, bool IsLowRes, bool Broadcast);

	void DetermineLength();

	FSongMaps                 DefaultMaps;
	UMidiFile::FMidiTrackList DefaultTracks;

	TWeakPtr<FMidiPlayCursorMgr> TimeAuthority;

	TSharedPtr<FMidiFileData> MidiFileData;
	const FSongMaps* SongMaps;     // <-- This is a pointer to the actual midi song map data... allows for fast access

	mutable FCriticalSection LowResCursorListLock;
	mutable FCriticalSection HiResCursorListLock;
	mutable bool TraversingHiResCursors  = false;
	mutable bool TraversingLowResCursors = false;
	float LengthMs;
	int32 LengthTicks;
	bool  DirectMappedTimeFollower;
	bool  Loop;
	float LoopOffsetTick;
	float LoopStartMs;
	int32 LoopStartTick;
	float LoopEndMs;
	int32 LoopEndTick;
	float MsSinceLowResUpdate;
	bool  HiResLoopedSinceLastLoResUpdate;
	bool  InMidiChangeLock;
	float CurrentAdvanceRate;
};

