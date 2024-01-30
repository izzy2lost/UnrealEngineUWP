// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

STORMSYNCAVABRIDGE_API DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncAvaBridge, Display, All);

#define STORM_SYNC_AVA_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncAvaBridge, Verbosity, Format, ##__VA_ARGS__); \
}
