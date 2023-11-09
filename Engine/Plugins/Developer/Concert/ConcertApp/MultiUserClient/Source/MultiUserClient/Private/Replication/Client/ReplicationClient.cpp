// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Submission/Data/AuthoritySubmission.h"

namespace UE::MultiUserClient
{
	FReplicationClient::FReplicationClient(
		const FGuid& EndpointId,
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TUniquePtr<IClientAuthoritySynchronizer> InAuthoritySynchronizer,
		TFunctionRef<FMakeSubmissionWorkflow> MakeSubmissionWorkflowFunc
		)
		: EndpointId(EndpointId)
		, ClientContentStorage(&InSessionContent)
		, StreamSynchronizer(MoveTemp(InStreamSynchronizer))
		, AuthoritySynchronizer(MoveTemp(InAuthoritySynchronizer))
		, LocalClientEditModel(ConcertClientSharedSlate::CreatePropertySelectionModel(
			*ClientContentStorage->Stream,
			ClientContentStorage->Stream->MakeReplicationMapGetterAttribute()
			))
		, LocalClientStreamDiffer(
			GetStreamSynchronizer(),
			ClientContentStorage->Stream->MakeReplicationMapGetterAttribute(),
			FStreamChangeTracker::FOnModifyReplicationMap::CreateLambda([this](){ ClientContentStorage->Stream->Modify(); })
			)
		, LocalAuthorityDiffer(EndpointId, *AuthoritySynchronizer, InAuthorityCache)
		, SubmissionWorkflow(MakeSubmissionWorkflowFunc(LocalClientStreamDiffer, LocalAuthorityDiffer, *StreamSynchronizer.Get()))
		, AutoSubmissionPolicy(*SubmissionWorkflow.Get(), LocalClientEditModel.Get(), LocalAuthorityDiffer)
	{
		LocalClientEditModel->OnObjectsChanged().AddRaw(this, &FReplicationClient::OnObjectsChanged);
		LocalClientEditModel->OnPropertiesChanged().AddRaw(this, &FReplicationClient::OnPropertiesChanged);
		
		LocalClientStreamDiffer.OnChangesReverted_GameThread().AddLambda([this]()
		{
			OnModelExternallyChangedDelegate.Broadcast();
		});

		SubmissionWorkflow->OnAuthorityRequestCompleted().AddRaw(this, &FReplicationClient::OnAuthoritySubmissionCompleted);
	}

	void FReplicationClient::OnObjectsChanged(
		TConstArrayView<UObject*> AddedObjects,
		TConstArrayView<FSoftObjectPath> RemovedObjects,
		ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason
		)
	{
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		// This must be done before SetAuthorityIfAllowed because it uses the cache for checking whether the object has properties assigned
		LocalClientStreamDiffer.RefreshChangesCache();
		
		// Better UX for user: automatically take authority for newly added objects (but only if it is allowed and causes no conflicts)
		TArray<FSoftObjectPath> ObjectPaths;
		Algo::Transform(AddedObjects, ObjectPaths, [](const UObject* Object){ return FSoftObjectPath(Object); });
		LocalAuthorityDiffer.SetAuthorityIfAllowed(ObjectPaths, true);

		// Refresh because authority changes may no longer be valid after modifying the stream
		LocalAuthorityDiffer.RefreshChanges();
	}

	void FReplicationClient::OnPropertiesChanged()
	{
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		LocalClientStreamDiffer.RefreshChangesCache();
		// Refresh because authority changes may no longer be valid after modifying the stream
		LocalAuthorityDiffer.RefreshChanges();
	}

	void FReplicationClient::OnAuthoritySubmissionCompleted(const FSubmitAuthorityChangesRequest& Request, const FSubmitAuthorityChangesResponse& Response)
	{
		if (!Response.Response)
		{
			return;
		}

		// Use case: You and another client submit at the same time. You lose. Revert your local changes so the checkboxes accurately reflect the authority state.
		for (const TPair<FSoftObjectPath, FConcertStreamArray>& RejectedObjectPair : Response.Response.GetValue().RejectedObjects)
		{
			LocalAuthorityDiffer.ClearAuthorityChange({ RejectedObjectPair.Key });
		}
	}
}
