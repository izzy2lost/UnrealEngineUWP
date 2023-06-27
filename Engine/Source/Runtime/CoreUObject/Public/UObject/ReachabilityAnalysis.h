// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ReachabilityAnalysis.h: Utility functions for configuring and running
	Incremental reachability analysis
=============================================================================*/

#pragma once

#include "CoreMinimal.h"


/**
 * Enables or disables incremental reachability analysis.
 *
 * @param	bEnabled True if incremental reachability analysis is to be enabled
 */
COREUOBJECT_API void SetIncrementalReachabilityAnalysisEnabled(bool bEnabled);

/**
 * Returns true if incremental reachability analysis is enabled
 */
COREUOBJECT_API bool GetIncrementalReachabilityAnalysisEnabled();

/**
 * Sets time limit for incremental rachability analysis (if enabled).
 *
 * @param	TimeLimitSeconds Time limit (in seconds) for incremental raachability analysis
 */
COREUOBJECT_API void SetReachabilityAnalysisTimeLimit(float TimeLimitSeconds);

/**
 * Returns the time limit for incremental rachability analysis.
 *
 * @return	time limit (in seconds) for incremental rachability analysis.
 */
COREUOBJECT_API float GetReachabilityAnalysisTimeLimit();
