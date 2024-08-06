// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "LiveLinkHubEditorSettings.generated.h"

/**
 * Settings for LiveLinkHub in editor.
 */
UCLASS(config = Editor, defaultconfig)
class LIVELINKHUBEDITOR_API ULiveLinkHubEditorSettings : public UObject
{
public:
	GENERATED_BODY()

	/** Whether to find the livelinkhub executable by looking up a registry key. */
	UPROPERTY(config)
	bool bDetectLiveLinkHubExecutable = false;

	/** Whether to find the livelinkhub executable by looking up a registry key. */
	UPROPERTY(config)
	bool bWriteLiveLinkHubRegistryKey = false;
};