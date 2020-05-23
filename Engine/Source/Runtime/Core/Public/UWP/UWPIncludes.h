// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	UWPIncludes.h: Includes the platform specific headers for UWP
==================================================================================*/

#pragma once

// Set up compiler pragmas, etc
#include "UWP/UWPCompilerSetup.h"

// include platform implementations
#include "UWP/UWPMemory.h"
#include "UWP/UWPString.h"
#include "UWP/UWPMisc.h"
#include "UWP/UWPStackWalk.h"
#include "UWP/UWPMath.h"
#include "UWP/UWPTime.h"
#include "UWP/UWPProcess.h"
#include "UWP/UWPOutputDevices.h"
#include "UWP/UWPAtomics.h"
#include "UWP/UWPTLS.h"
#include "UWP/UWPSplash.h"
#include "UWP/UWPSurvey.h"

typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;

typedef FGenericPlatformAffinity FPlatformAffinity;

#include "UWPProperties.h"
