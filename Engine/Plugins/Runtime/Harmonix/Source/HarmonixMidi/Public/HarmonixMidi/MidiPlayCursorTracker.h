// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/IntrusiveDoubleLinkedList.h"
#include "Misc/ScopeLock.h"

class FMidiPlayCursor;

#ifndef HARMONIX_MIDIPLAYCURSOR_ENABLE_ENSURE_OWNER
#define HARMONIX_MIDIPLAYCURSOR_ENABLE_ENSURE_OWNER DO_ENSURE
#endif

#if HARMONIX_MIDIPLAYCURSOR_ENABLE_ENSURE_OWNER
class FHarmonixMidiPlayCursorDebugCriticalSection
{
public:
	FORCEINLINE void Lock()
	{
		CriticalSection.Lock();
		++RecursionCount;
	}

	FORCEINLINE bool TryLock()
	{
		if (CriticalSection.TryLock())
		{
			++RecursionCount;
			return true;
		}
		return false;
	}

	FORCEINLINE void Unlock()
	{
		--RecursionCount;
		CriticalSection.Unlock();
	}

	FORCEINLINE int32 GetRecursionCountIfOwned()
	{
		if (CriticalSection.TryLock())
		{
			int32 Count = RecursionCount;
			CriticalSection.Unlock();
			return Count;
		}
		return 0;
	}

private:
	FCriticalSection CriticalSection;
	int32 RecursionCount = 0;
};

using FMidiPlayCursorListCS = FHarmonixMidiPlayCursorDebugCriticalSection;
#else
using FMidiPlayCursorListCS = FCriticalSection;
#endif

using FMidiPlayCursorListLock = UE::TScopeLock<FMidiPlayCursorListCS>;

struct FMidiPlayCursorTracker
{
	mutable FMidiPlayCursorListCS CursorListCS;

	int32   CurrentTick; // We've broadcast all events up through this tick
	float   CurrentMs;   // We've broadcast all events up through this Ms
	float   ElapsedMs;
	int32   LoopCount;

	int32   EarliestCursorTick;
	int32   LatestCursorTick;
	float   EarliestCursorMs;
	float   LatestCursorMs;

	float   CurrentAdvanceRate;

	float   LoopOffsetTick;
	float   LoopStartMs;
	int32   LoopStartTick;
	float   LoopEndMs;
	int32   LoopEndTick;
	bool    Loop;
	bool    LoopIgnoringLookAhead;

	mutable bool TraversingCursors;

	const bool IsLowRes;

	TIntrusiveDoubleLinkedList<FMidiPlayCursor> Cursors;

	HARMONIXMIDI_API explicit FMidiPlayCursorTracker(bool InIsLowRes);

	HARMONIXMIDI_API bool IsAtStart();
	HARMONIXMIDI_API void AddCursor(FMidiPlayCursor* Cursor);
	HARMONIXMIDI_API bool ContainsCursor(FMidiPlayCursor* Cursor);
	HARMONIXMIDI_API bool RemoveCursor(FMidiPlayCursor* Cursor);
	HARMONIXMIDI_API void Clear();

	HARMONIXMIDI_API void RecalculateExtents();
	HARMONIXMIDI_API int32 GetFarthestAheadCursorTick() const;
	HARMONIXMIDI_API int32 GetFarthestBehindCursorTick() const;

	HARMONIXMIDI_API void MoveToLoopStart(int32 NewThruTick, float NewThruMs);

	bool HasQueuedReset = true;
	int32 NewQueuedTick = 0;
	float NewQueuedMs = 0.0f;
	int32 NewQueuedPreRollTick = 0;
	float NewQueuedPreRollMs = 0.0f;
	bool  NewQueuedBroadcast = false;

	HARMONIXMIDI_API void QueueReset(int32 NewCurrentTick, float NewCurrentMs, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast);
	HARMONIXMIDI_API void HandleQueuedReset();
	HARMONIXMIDI_API void Reset(int32 NewCurrentTick, float NewCurrentMs, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast);
	HARMONIXMIDI_API void ResetNewCursor(FMidiPlayCursor* Cursor, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast);
	HARMONIXMIDI_API void Reset(int32 NewCurrentTick, float NewCurrentMs, bool Broadcast);
};

