// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

STORMSYNCCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncCore, Display, All);

#define STORM_SYNC_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncCore, Verbosity, Format, ##__VA_ARGS__); \
}
