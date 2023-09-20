// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"

enum class EBreakBehavior : uint8;

struct FConcertChangeStream_Request;
struct FObjectInStreamID;
struct FReplicationStreamDescription;

/** This namespace contains the shared logic for applying FConcertChangeStream_Requests. */
namespace UE::ConcertSyncCore::Replication::ChangeStreamUtils
{
	/**
	 * Iterates through all objects that are removed and therefore the requesting client would lose authority over.
	 * 
	 * @param Request The request to parse
	 * @param ExistingStreams The streams the request would be applied to
	 * @param Callback The callback to process the objects
	 */
	CONCERTSYNCCORE_API void ForEachObjectLosingAuthority(
		const FConcertChangeStream_Request& Request,
		const TArray<FReplicationStreamDescription>& ExistingStreams,
		TFunctionRef<EBreakBehavior(const FObjectInStreamID&)> Callback
		);
	
	/**
	 * Modifies RegisteredStreams as described by the request without validating that the Request is valid to apply.
	 * @param Request The request to parse
	 * @param StreamsToModify The streams to apply the request to
	 */
	CONCERTSYNCCORE_API void ApplyValidatedRequest(const FConcertChangeStream_Request& Request, IN OUT TArray<FReplicationStreamDescription>& StreamsToModify);
}
