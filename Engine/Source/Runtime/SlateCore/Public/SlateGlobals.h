// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Globals
 *****************************************************************************/

SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlate, Log, All);
SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlateStyles, Log, All);

DECLARE_STATS_GROUP(TEXT("Slate Memory"), STATGROUP_SlateMemory, STATCAT_Advanced);
DECLARE_STATS_GROUP(TEXT("Slate"), STATGROUP_Slate, STATCAT_Advanced);
DECLARE_STATS_GROUP_VERBOSE(TEXT("SlateVerbose"), STATGROUP_SlateVerbose, STATCAT_Advanced);


// Compile all the RichText and MultiLine editable text?
#define WITH_FANCY_TEXT 1

/* Forward declarations
*****************************************************************************/
class FActiveTimerHandle;
// @ATG_CHANGE : BEGIN UWP support (working around /ZW x86 pack value issue)
#if PLATFORM_UWP
PACK_WINRT()
#endif
enum class EActiveTimerReturnType : uint8;
#if PLATFORM_UWP
PACK_WINRT_REVERT()
#endif
// @ATG_CHANGE : END