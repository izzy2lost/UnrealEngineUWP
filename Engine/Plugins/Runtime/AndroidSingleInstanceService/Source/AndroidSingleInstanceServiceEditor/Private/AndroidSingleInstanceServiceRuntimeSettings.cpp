// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidSingleInstanceServiceRuntimeSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AndroidSingleInstanceServiceRuntimeSettings)

//////////////////////////////////////////////////////////////////////////
// UAndroidSingleInstanceServiceRuntimeSettings

UAndroidSingleInstanceServiceRuntimeSettings::UAndroidSingleInstanceServiceRuntimeSettings(const FObjectInitializer& ObjectInitializer)
        : Super(ObjectInitializer)
        , bEnablePlugin(false)
		, bCompileASISProject(false)
{
}