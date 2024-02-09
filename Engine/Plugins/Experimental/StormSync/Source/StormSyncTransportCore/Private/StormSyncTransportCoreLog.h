// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncTransportCore, Display, All);

#define STORM_SYNC_CORE_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncTransportCore, Verbosity, Format, ##__VA_ARGS__); \
}
