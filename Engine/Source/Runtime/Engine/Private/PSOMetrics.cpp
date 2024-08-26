// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PSOMetrics.cpp: 
=============================================================================*/

#include "PSOMetrics.h"
#include "HAL/CriticalSection.h"

namespace PSOMetrics
{
	FCriticalSection PSOCriticalSection;
	float DurationSum = 0;
	int Count = 0;
}

static FPSOMetricsEvent PSOMetricsEvent;
FPSOMetricsEvent& GetPSOMetricsDelegate()
{
	if (!PSOMetricsEvent.IsBound())
	{
		PSOMetricsEvent.BindLambda([](float CompilationDuration)
		{
			// we need this scope because this will be called from RHIThread
			FScopeLock ScopeLock(&PSOMetrics::PSOCriticalSection);

			PSOMetrics::DurationSum += CompilationDuration;
			++PSOMetrics::Count;
		});
	}

	return PSOMetricsEvent;
}

void GetPSOCompilationMetrics(float& DurationSum, int& Count)
{
	// we need this scope because this will be called from the GameThread
	FScopeLock ScopeLock(&PSOMetrics::PSOCriticalSection);

	DurationSum = PSOMetrics::DurationSum;
	Count = PSOMetrics::Count;

	DurationSum = 0;
	Count = 0;
}