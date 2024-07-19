// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"

#include "Containers/Array.h"
#include "HAL/Platform.h"

namespace UE::MultiUserClient
{
	/** Describes why changing a client failed. */
	struct FFailToReplaceWithPresetInfo
	{
		FConcertSessionClientInfo ClientInfo;
	};

	/** Describes the outcome of replacing a session's content with a preset. */
	struct FReplacedWithPresetEvent
	{
		/** The clients for which an error occured. */
		TArray<FFailToReplaceWithPresetInfo> FailedClients;

		bool IsSuccess() const { return FailedClients.IsEmpty(); }
	};
}

