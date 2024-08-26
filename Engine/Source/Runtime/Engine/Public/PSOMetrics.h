// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PSOMetrics.h
=============================================================================*/

#pragma once

#include "Delegates/Delegate.h"

/**
 * PSO metric delegate is called for each individual PSO compilation.
 * At the moment only IOS and Android platforms will call this.
 */ 
DECLARE_DELEGATE_OneParam(FPSOMetricsEvent, float /*CompilationDuration*/);
extern ENGINE_API FPSOMetricsEvent& GetPSOMetricsDelegate();

// retrieves the current metrics and set it to zero
extern ENGINE_API void GetPSOCompilationMetrics(float& DurationSum, int& Count);