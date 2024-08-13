// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/CookGarbageCollect.h"

#include "UObject/GarbageCollectionHistory.h"

namespace UE::Cook
{

FCookGCDiagnosticContext::~FCookGCDiagnosticContext()
{
	SetGCWithHistoryRequested(false);
}

bool FCookGCDiagnosticContext::NeedsDiagnosticSecondGC() const
{
	return bRequestGCWithHistory;
}

bool FCookGCDiagnosticContext::CurrentGCHasHistory() const
{
	return bCurrentGCHasHistory;
}

bool FCookGCDiagnosticContext::TryRequestGCWithHistory()
{
#if ENABLE_GC_HISTORY
	if (!bRequestsAvailable || !bGCInProgress || bCurrentGCHasHistory)
	{
		return false;
	}
	SetGCWithHistoryRequested(true);
	return true;
#else
	return false;
#endif
}

void FCookGCDiagnosticContext::OnCookerStartCollectGarbage()
{
	bRequestsAvailable = true;

	bGCInProgress = true;
#if ENABLE_GC_HISTORY
	bCurrentGCHasHistory = FGCHistory::Get().GetHistorySize() > 0;
#else
	bCurrentGCHasHistory = false;
#endif
}

void FCookGCDiagnosticContext::OnCookerEndCollectGarbage()
{
	bGCInProgress = false;
	bCurrentGCHasHistory = false;
}

void FCookGCDiagnosticContext::OnEvaluateResultsComplete()
{
	SetGCWithHistoryRequested(false);
}

void FCookGCDiagnosticContext::SetGCWithHistoryRequested(bool bValue)
{
#if ENABLE_GC_HISTORY
	if (bValue == bRequestGCWithHistory)
	{
		return;
	}

	if (bValue)
	{
		SavedGCHistorySize = FGCHistory::Get().GetHistorySize();
		if (SavedGCHistorySize < 1)
		{
			FGCHistory::Get().SetHistorySize(1);
		}
	}
	else
	{
		if (SavedGCHistorySize != FGCHistory::Get().GetHistorySize())
		{
			FGCHistory::Get().SetHistorySize(SavedGCHistorySize);
		}
		SavedGCHistorySize = 0;
	}
	bRequestGCWithHistory = bValue;
#endif
}

} // namespace UE::Cook