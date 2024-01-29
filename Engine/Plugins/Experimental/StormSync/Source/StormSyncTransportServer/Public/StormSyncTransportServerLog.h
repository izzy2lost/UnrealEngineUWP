// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

STORMSYNCTRANSPORTSERVER_API DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncServer, Display, All);

#define STORM_SYNC_SERVER_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncServer, Verbosity, Format, ##__VA_ARGS__); \
}
