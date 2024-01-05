// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientChangeConversionUtils.h"

#include "Replication/ChangeOperationTypes.h"
#include "Replication/Util/StreamRequestUtils.h"

#include "Algo/RemoveIf.h"
#include "Misc/Guid.h"
#include "Replication/Submission/Data/AuthoritySubmission.h"
#include "Replication/Submission/Data/StreamSubmission.h"

namespace UE::MultiUserClient::ClientChangeConversionUtils
{
	namespace Private
	{
		static TOptional<FConcertReplication_ChangeStream_PutObject> GeneratePutRequest(
			const UObject& Object,
			FPropertyChange&& PropertyChange,
			const FObjectReplicationMap& ClientStreamContent
			)
		{
			// TODO UE-201166: This step could be simplified if we had an append operation in FConcertReplication_ChangeStream_PutObject
			const FReplicatedObjectInfo* ExistingObjectInfo = ClientStreamContent.ReplicatedObjects.Find(&Object);
			if (ExistingObjectInfo)
			{
				FReplicatedObjectInfo NewObjectInfo = FReplicatedObjectInfo::Make(Object);
				TArray<FConcertPropertyChain>& ReplicatedProperties = NewObjectInfo.PropertySelection.ReplicatedProperties;
				switch (PropertyChange.ChangeType)
				{
				case EPropertyChangeType::Put:
					ReplicatedProperties = MoveTemp(PropertyChange.Properties);
					break;
				case EPropertyChangeType::Add:
					ReplicatedProperties.Append(PropertyChange.Properties);
					break;
				case EPropertyChangeType::Remove:
					ReplicatedProperties.SetNum(Algo::RemoveIf(ReplicatedProperties, [&PropertyChange](const FConcertPropertyChain& Property)
					{
						return PropertyChange.Properties.Contains(Property);
					}));
					break;
				default: checkNoEntry(); break;
				}
				return FConcertReplication_ChangeStream_PutObject::MakeFromChange(*ExistingObjectInfo, NewObjectInfo);
			}
			else
			{
				FReplicatedObjectInfo NewObjectInfo = FReplicatedObjectInfo::Make(Object);
				TArray<FConcertPropertyChain>& ReplicatedProperties = NewObjectInfo.PropertySelection.ReplicatedProperties;
				switch (PropertyChange.ChangeType)
				{
				case EPropertyChangeType::Put:
				case EPropertyChangeType::Add:
					ReplicatedProperties = MoveTemp(PropertyChange.Properties);
					return FConcertReplication_ChangeStream_PutObject::MakeFromInfo(NewObjectInfo);
				case EPropertyChangeType::Remove:
					return {};
				default: checkNoEntry(); break;
				}
			}
				
			return {};
		}

		static void BuildStreamChanges(
			TMap<UObject*, FPropertyChange>&& PropertyChanges,
			const FGuid& ClientStreamId,
			const FObjectReplicationMap& ClientStreamContent,
			FStreamChangelist& StreamChangelist
			)
		{
			for (TPair<UObject*, FPropertyChange>& PropertyChange : PropertyChanges)
			{
				UObject* Object = PropertyChange.Key; 
				if (!Object)
				{
					continue;
				}
				
				TOptional<FConcertReplication_ChangeStream_PutObject> PutRequest = GeneratePutRequest(*Object, MoveTemp(PropertyChange.Value), ClientStreamContent);
				if (PutRequest)
				{
					// Gracefully handle APi caller (e.g. Blueprinter) forgetting to specify parent properties,
					// e.g. if they only specify ["Vector", "X"], we'll add ["Vector"] as well because it is needed for proper replication. 
					PutRequest->Properties.DiscoverAndAddImplicitParentProperties();
					
					StreamChangelist.ObjectsToPut.Add(
						FObjectInStreamID { ClientStreamId, Object },
						MoveTemp(*PutRequest)
					);
				}
			}
		}

		static void BuildAuthorityChanges(TSet<FSoftObjectPath>&& ObjectsToRemove, const FGuid& ClientStreamId, FStreamChangelist& StreamChangelist)
		{
			for (FSoftObjectPath& RemovedObject : ObjectsToRemove)
			{
				StreamChangelist.ObjectsToRemove.Emplace(
					FObjectInStreamID{ ClientStreamId, MoveTemp(RemovedObject) }
				);
			}
		}
	}

	TOptional<FConcertReplication_ChangeStream_Request> Transform(
		FChangeStreamRequest Request,
		const FGuid& ClientStreamId,
		const FObjectReplicationMap& ClientStreamContent
		)
	{
		FStreamChangelist StreamChangelist;
		Private::BuildStreamChanges(MoveTemp(Request.PropertyChanges), ClientStreamId, ClientStreamContent, StreamChangelist);
		Private::BuildAuthorityChanges(MoveTemp(Request.ObjectsToRemove), ClientStreamId, StreamChangelist);
		if (StreamChangelist.ObjectsToPut.IsEmpty() && StreamChangelist.ObjectsToRemove.IsEmpty())
		{
			return {};
		}
		
		const bool bNeedsToRegisterStream = ClientStreamContent.IsEmpty();
		return bNeedsToRegisterStream
			? StreamRequestUtils::BuildChangeRequest_CreateNewStream(ClientStreamId, StreamChangelist)
			: StreamRequestUtils::BuildChangeRequest_UpdateExistingStream(ClientStreamId, MoveTemp(StreamChangelist));
	}

	TOptional<FConcertReplication_ChangeAuthority_Request> Transform(
		FChangeAuthorityRequest Request,
		const FGuid& ClientStreamId
		)
	{
		FConcertReplication_ChangeAuthority_Request Result;
		for (FSoftObjectPath& TakeAuthority : Request.ObjectsToStartReplicating)
		{
			Result.TakeAuthority.Emplace(MoveTemp(TakeAuthority))
				.StreamIds = { { ClientStreamId } };
		}
		for (FSoftObjectPath& ReleaseAuthority : Request.ObjectToStopReplicating)
		{
			Result.ReleaseAuthority.Emplace(MoveTemp(ReleaseAuthority))
				.StreamIds = { { ClientStreamId } };
		}
		return Result;
	}

	EChangeStreamOperationResult Transform(const FSubmitStreamChangesResponse& Response)
	{
		switch (Response.ErrorCode)
		{
		case EStreamSubmissionErrorCode::Success: return EChangeStreamOperationResult::Success;
		case EStreamSubmissionErrorCode::NoChange: return EChangeStreamOperationResult::NoChanges; 
		case EStreamSubmissionErrorCode::Timeout: return EChangeStreamOperationResult::Timeout; 
		case EStreamSubmissionErrorCode::Cancelled: return EChangeStreamOperationResult::Cancelled;
		default:
			checkNoEntry();
			return EChangeStreamOperationResult::Cancelled;
		}
	}
	
	EChangeAuthorityOperationResult Transform(const FSubmitAuthorityChangesResponse& Response)
	{
		switch (Response.ErrorCode)
		{
		case EAuthoritySubmissionResponseErrorCode::Success: return EChangeAuthorityOperationResult::Success;
		case EAuthoritySubmissionResponseErrorCode::NoChange: return EChangeAuthorityOperationResult::NoChanges; 
		case EAuthoritySubmissionResponseErrorCode::Timeout: return EChangeAuthorityOperationResult::Timeout; 
		case EAuthoritySubmissionResponseErrorCode::CancelledDueToStreamUpdate: return EChangeAuthorityOperationResult::CancelledDueToStreamUpdate; 
		case EAuthoritySubmissionResponseErrorCode::Cancelled: return EChangeAuthorityOperationResult::Cancelled;
		default:
			checkNoEntry();
			return EChangeAuthorityOperationResult::Cancelled;
		}
	}
}
