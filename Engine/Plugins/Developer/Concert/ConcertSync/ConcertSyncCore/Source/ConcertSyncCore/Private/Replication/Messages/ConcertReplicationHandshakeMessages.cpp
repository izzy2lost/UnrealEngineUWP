// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Messages/ConcertReplicationHandshakeMessages.h"

namespace UE::ConcertSyncCore::Replication
{
	FString LexJoinErrorCode(EJoinReplicationErrorCode ErrorCode)
	{
		static_assert(static_cast<int32>(EJoinReplicationErrorCode::Max) == 10, "Update JoinErrorCodeToText when you change entries");

		switch (ErrorCode)
		{
		case EJoinReplicationErrorCode::Success: return TEXT("Success");
		case EJoinReplicationErrorCode::NetworkError: return TEXT("NetworkError");
		case EJoinReplicationErrorCode::Cancelled: return TEXT("Cancelled");
		case EJoinReplicationErrorCode::AlreadyInProgress: return TEXT("AlreadyInProgress");
		case EJoinReplicationErrorCode::NotInAnyConcertSession: return TEXT("NotInAnyConcertSession");
		case EJoinReplicationErrorCode::InvalidClass: return TEXT("InvalidClass");
		case EJoinReplicationErrorCode::DuplicateProperty: return TEXT("DuplicateProperty");
		case EJoinReplicationErrorCode::AlreadyInSession: return TEXT("AlreadyInSession");
		case EJoinReplicationErrorCode::ConflictingStreamId: return TEXT("ConflictingStreamId");
		case EJoinReplicationErrorCode::ConflictingAuthority: return TEXT("ConflictingAuthority");
		case EJoinReplicationErrorCode::FailedToUnpackStream: return TEXT("FailedToUnpackStream");
		default: 
			return FString();
		}
	}
}