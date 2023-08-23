// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"

namespace UE::ConcertSyncClient::Replication
{
	struct FAuthorityChangeRequest;
	struct FAuthorityChangeResponse;

	/** Creates a fullfilled future which rejects all items in FConcertChangeAuthority_Request::TakeAuthority. */
	TFuture<FAuthorityChangeResponse> RejectAll(FAuthorityChangeRequest&& Args);
};
