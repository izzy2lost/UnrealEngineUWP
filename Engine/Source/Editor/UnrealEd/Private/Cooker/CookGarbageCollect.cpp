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
	if (!bRequestsAvailable || !bGCInProgress || bCurrentGCHasHistory)
	{
		return false;
	}
	SetGCWithHistoryRequested(true);
	return true;
}

void FCookGCDiagnosticContext::OnCookerStartCollectGarbage()
{
	bRequestsAvailable = true;

	bGCInProgress = true;
	bCurrentGCHasHistory = FGCHistory::Get().GetHistorySize() > 0;
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
}

} // namespace UE::Cook