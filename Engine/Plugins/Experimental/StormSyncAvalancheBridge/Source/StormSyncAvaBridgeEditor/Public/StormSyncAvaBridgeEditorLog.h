// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

STORMSYNCAVABRIDGEEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogStormSyncAvaBridgeEditor, Display, All);

#define STORM_SYNC_AVA_EDITOR_LOG(Verbosity, Format, ...) \
{ \
    UE_LOG(LogStormSyncAvaBridgeEditor, Verbosity, Format, ##__VA_ARGS__); \
}
