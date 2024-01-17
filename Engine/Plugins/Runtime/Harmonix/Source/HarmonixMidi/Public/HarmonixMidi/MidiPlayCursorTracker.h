// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/IntrusiveDoubleLinkedList.h"

class FMidiPlayCursor;

struct HARMONIXMIDI_API FMidiPlayCursorTracker
{
	int32   CurrentTick; // We've broadcast all events up through this tick
	float   CurrentMs;   // We've broadcast all events up through this Ms
	float   ElapsedMs;
	int32   LoopCount;

	int32   EarliestCursorTick;
	int32   LatestCursorTick;
	float   EarliestCursorMs;
	float   LatestCursorMs;

	TIntrusiveDoubleLinkedList<FMidiPlayCursor> Cursors;

	FMidiPlayCursorTracker();

	bool IsAtStart();
	void AddCursor(FMidiPlayCursor* Cursor);
	bool ContainsCursor(FMidiPlayCursor* Cursor);
	bool RemoveCursor(FMidiPlayCursor* Cursor);
	void Clear();

	void RecalculateExtents();
	int32 GetFarthestAheadCursorTick() const;
	int32 GetFarthestBehindCursorTick() const;

	void MoveToLoopStart(int32 NewThruTick, float NewThruMs);

	bool HasQueuedReset = true;
	int32 NewQueuedTick = 0;
	float NewQueuedMs = 0.0f;
	int32 NewQueuedPreRollTick = 0;
	float NewQueuedPreRollMs = 0.0f;
	bool  NewQueuedBroadcast = false;

	void QueueReset(int32 NewCurrentTick, float NewCurrentMs, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast);
	void HandleQueuedReset();
	void Reset(int32 NewCurrentTick, float NewCurrentMs, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast);
	void ResetNewCursor(FMidiPlayCursor* Cursor, int32 PreRollStartTick, float PreRollStartMs, bool Broadcast);
	void Reset(int32 NewCurrentTick, float NewCurrentMs, bool Broadcast);
};

