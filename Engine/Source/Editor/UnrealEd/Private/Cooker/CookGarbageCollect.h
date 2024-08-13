// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::Cook
{

/**
 * Holds information about the cookers garbage collection status, and communicates requests from low level structures
 * back up to the CookCommandlet that is capable of acting on those requests with additional garbage collection
 * commands.
 */
class FCookGCDiagnosticContext
{
public:
	~FCookGCDiagnosticContext();

	bool NeedsDiagnosticSecondGC() const;
	bool CurrentGCHasHistory() const;

	/**
	 * Add a request to reexecute the current GC after all of the PostGarbageCollect calls run and
	 * control returns back to the caller of CollectGarbage. Returns false if not currently in post-GC,
	 * or the garbage collect that just ran already had history.
	 */
	bool TryRequestGCWithHistory();

	void OnCookerStartCollectGarbage();
	void OnCookerEndCollectGarbage();
	void OnEvaluateResultsComplete();

private:
	void SetGCWithHistoryRequested(bool bValue);
	
	int32 SavedGCHistorySize = 0;
	bool bRequestsAvailable = false;
	bool bGCInProgress = false;
	bool bRequestGCWithHistory = false;
	bool bCurrentGCHasHistory = false;
};

} // namespace UE::Cook
