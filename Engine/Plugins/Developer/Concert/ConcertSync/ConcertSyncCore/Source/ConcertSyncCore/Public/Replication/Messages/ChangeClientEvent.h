// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChangeAuthority.h"
#include "ChangeStream.h"
#include "ChangeClientEvent.generated.h"

/**
 * Sent by server to notify a client that their stream content and / or authority has been changed by an external entity,
 * i.e. when the change was not initiated by the client itself.
 * 
 * For now, this is only in response to a remote client sending a FConcertReplication_PutState_Request but in the future it could also be
 * the user changing client content via the server UI.
 */
USTRUCT()
struct FConcertReplication_ChangeClientEvent
{
	GENERATED_BODY()

	/** The change made to the client's streams. */
	UPROPERTY()
	FConcertReplication_ChangeStream_Request StreamChange;

	/** The change made to the client's authority. */
	UPROPERTY()
	FConcertReplication_ChangeAuthority_Request AuthorityChange;

	/** The change made to the client's sync control in response to the above changes. */
	UPROPERTY()
	FConcertReplication_ChangeSyncControl SyncControlChange;
};