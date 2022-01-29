// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once


// Pre-system API include
#include "UWP/PreUWPApi.h"

#ifndef STRICT
#define STRICT
#endif

// System API include
#include "UWP/MinUWPApi.h"

// Post-system API include
#include "UWP/PostUWPApi.h"

// Set up compiler pragmas, etc
#include "UWP/UWPPlatformCompilerSetup.h"

// Macro for releasing COM objects
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }
// SIMD intrinsics
#include <intrin.h>

#include <intsafe.h>
#include <strsafe.h>
#include <tchar.h>

#include <stdint.h>
#include <concrt.h>

