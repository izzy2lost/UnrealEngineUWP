// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncDrives, Display, All);

#define STORM_SYNC_DRIVES_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncDrives, Verbosity, Format, ##__VA_ARGS__); \
}
