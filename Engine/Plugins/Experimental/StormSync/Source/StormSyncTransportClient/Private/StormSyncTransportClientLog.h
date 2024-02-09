// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncClient, Display, All);

#define STORM_SYNC_CLIENT_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncClient, Verbosity, Format, ##__VA_ARGS__); \
}
