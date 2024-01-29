// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

STORMSYNCEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncEditor, Display, All);

#define STORM_SYNC_EDITOR_LOG(Verbosity, Format, ...) \
{ \
	UE_LOG(LogStormSyncEditor, Verbosity, Format, ##__VA_ARGS__); \
}
