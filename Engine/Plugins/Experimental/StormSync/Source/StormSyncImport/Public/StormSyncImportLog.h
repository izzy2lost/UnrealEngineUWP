// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

STORMSYNCIMPORT_API DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncImport, Display, All);

#define STORM_SYNC_IMPORT_LOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogStormSyncImport, Verbosity, Format, ##__VA_ARGS__); \
}
