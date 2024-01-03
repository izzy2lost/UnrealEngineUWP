// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertPropertyChainWrapper.h"
#include "UObject/SoftObjectPath.h"

#if WITH_CONCERT
#include "Replication/ChangeOperationTypes.h"
#endif

#include "ChangeClientBlueprintParams.generated.h"

/** Result of changing a client's stream */
UENUM(BlueprintType)
enum class EMultiUserChangeStreamOperationResult : uint8
{
	/** All changes made or none were requested. */
	Success,
	/** No changes were requested to be made */
	NoChanges,

	// Realistic failures
	
	/** The changes were rejected by the server. */
	Rejected,
	/** Failed because the request timed out. */
	Timeout,
	/** The submission progress was cancelled, e.g. because the target client disconnected. */
	Cancelled,
	
	/** Unlikely. ISubmissionWorkflow validation rejected the change. See log. Could be e.g. that the remote client does not allow remote editing. */
	FailedToSendRequest,

	// Bad API usage
	
	/** No submission took place because the local editor is not in any session. */
	NotInSession,
	/** No submission took place because the client ID was invalid.  */
	UnknownClient,
	/** The request was not made on the game thread. */
	NotOnGameThread,
	/** The MultiUserClient module is not available. Usually when you're in a runtime build but the operation can only be run in editor builds. */
	NotAvailable
};

/** Result of changing a client's authority */
UENUM(BlueprintType)
enum class EMultiUserChangeAuthorityOperationResult : uint8
{
	/** All changes made or none were requested. */
	Success,
	/** No changes were requested to be made */
	NoChanges,

	// Realistic failures

	/** The changes were rejected by the server. */
	Rejected,
	/** Failed because the request timed out. */
	Timeout,
	/** The authority change was not submitted because the stream change was unsuccessful. */
	CancelledDueToStreamUpdate,
	/** The submission progress was cancelled, e.g. because the target client disconnected. */
	Cancelled,
	
	/** Unlikely. ISubmissionWorkflow validation rejected the change. See log. Could be e.g. that the remote client does not allow remote editing. */
	FailedToSendRequest,

	// Bad API usage
	
	/** No submission took place because the local editor is not in any session. */
	NotInSession,
	/** No submission took place because the client ID was invalid.  */
	UnknownClient,
	/** The request was not made on the game thread. */
	NotOnGameThread,
	/** The MultiUserClient module is not available. Usually when you're in a runtime build but the operation can only be run in editor builds. */
	NotAvailable
};

UENUM(BlueprintType)
enum class EMultiUserPropertyChangeType : uint8
{
	/** Replace all assigned properties with the given properties. */
	Put,
	/** Append the given properties, keeping any preexisting properties. */
	Add,
	/** Remove the given properties. */
	Remove
};

USTRUCT(BlueprintType)
struct FMultiUserPropertyChange
{
	GENERATED_BODY()

	/** The properties for the object. See ChangeType for how they are used */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	TArray<FConcertPropertyChainWrapper> Properties;

	/** How you want Properties to be applied to the object. */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	EMultiUserPropertyChangeType ChangeType = EMultiUserPropertyChangeType::Add;
};

/** Params for changing a client's stream. */
USTRUCT(BlueprintType)
struct FMultiUserChangeStreamRequest
{
	GENERATED_BODY()
	
	/** Property changes to make to objects. */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	TMap<UObject*, FMultiUserPropertyChange> PropertyChanges;

	/** Objects that should be unregistered (they will also stop replicating if added here) */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	TSet<FSoftObjectPath> ObjectsToRemove;
	
	bool IsEmpty() const { return PropertyChanges.IsEmpty() && ObjectsToRemove.IsEmpty(); }
};

/** Params for changing a client's authority (what they're replicating). */
USTRUCT(BlueprintType)
struct FMultiUserChangeAuthorityRequest
{
	GENERATED_BODY()
	
	/** Objects that should start replicating. The objects must previously have been registered. */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	TSet<FSoftObjectPath> ObjectsToStartReplicating;

	/** Objects that should stop replicating. */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	TSet<FSoftObjectPath> ObjectToStopReplicating;

	bool IsEmpty() const { return ObjectsToStartReplicating.IsEmpty() && ObjectToStopReplicating.IsEmpty(); }
};

/** Params for changing a client's stream or authority. */
USTRUCT(BlueprintType)
struct FMultiUserChangeClientReplicationRequest
{
	GENERATED_BODY()
	
	/**
	 * Changes to make to the client's registered objects.
	 * Performed before AuthorityChangeRequest. If it fails, AuthorityChangeRequest will also fail.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	FMultiUserChangeStreamRequest StreamChangeRequest;
	
	/**
	 * Changes to make to the objects the client is replicating.
	 * Fails if StreamChangeRequest fails.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Multi-user")
	FMultiUserChangeAuthorityRequest AuthorityChangeRequest;
};

/** Result of processing a FChangeClientReplicationRequest. */
USTRUCT(BlueprintType)
struct FMultiUserChangeClientReplicationResult
{
	GENERATED_BODY()

	/** The result of changing streams. */
	UPROPERTY(BlueprintReadOnly, Category = "Multi-user")
	EMultiUserChangeStreamOperationResult StreamChangeResult = EMultiUserChangeStreamOperationResult::Success;

	/** The result of changing authority. Fails if StreamChangeResult fails. */
	UPROPERTY(BlueprintReadOnly, Category = "Multi-user")
	EMultiUserChangeAuthorityOperationResult AuthorityChangeResult = EMultiUserChangeAuthorityOperationResult::Success;
};

namespace UE::MultiUserClientLibrary
{
#if WITH_CONCERT
	MULTIUSERCLIENTLIBRARY_API EMultiUserChangeStreamOperationResult Transform(MultiUserClient::EChangeStreamOperationResult Data);
	MULTIUSERCLIENTLIBRARY_API EMultiUserChangeAuthorityOperationResult Transform(MultiUserClient::EChangeAuthorityOperationResult Data);
	MULTIUSERCLIENTLIBRARY_API FMultiUserChangeClientReplicationResult Transform(MultiUserClient::FChangeClientReplicationResult Data);
	
	MULTIUSERCLIENTLIBRARY_API MultiUserClient::EPropertyChangeType Transform(EMultiUserPropertyChangeType Data);
	MULTIUSERCLIENTLIBRARY_API MultiUserClient::FPropertyChange Transform(FMultiUserPropertyChange Data);
	
	MULTIUSERCLIENTLIBRARY_API MultiUserClient::FChangeStreamRequest Transform(FMultiUserChangeStreamRequest Data);
	MULTIUSERCLIENTLIBRARY_API MultiUserClient::FChangeAuthorityRequest Transform(FMultiUserChangeAuthorityRequest Data);
	MULTIUSERCLIENTLIBRARY_API MultiUserClient::FChangeClientReplicationRequest Transform(FMultiUserChangeClientReplicationRequest Data);
#endif
}