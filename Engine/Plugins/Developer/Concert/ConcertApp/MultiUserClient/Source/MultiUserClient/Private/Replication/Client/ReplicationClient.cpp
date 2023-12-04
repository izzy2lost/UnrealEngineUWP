// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClient.h"

#include "MultiUserReplicationSettings.h"
#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/StreamExtenderBySettings.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Submission/Data/AuthoritySubmission.h"

#include "Misc/CoreDelegates.h"

namespace UE::MultiUserClient
{
	FReplicationClient::FReplicationClient(
		const FGuid& EndpointId,
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TUniquePtr<IClientAuthoritySynchronizer> InAuthoritySynchronizer,
		TUniquePtr<ISubmissionWorkflow> InSubmissionWorkflow
		)
		: EndpointId(EndpointId)
		, ClientContentStorage(&InSessionContent)
		, StreamSynchronizer(MoveTemp(InStreamSynchronizer))
		, AuthoritySynchronizer(MoveTemp(InAuthoritySynchronizer))
		, SubmissionWorkflow(MoveTemp(InSubmissionWorkflow))
		, SubmissionQueue(*SubmissionWorkflow)
		, LocalClientEditModel(ConcertClientSharedSlate::CreatePropertySelectionModel(
			*ClientContentStorage->Stream,
			ClientContentStorage->Stream->MakeReplicationMapGetterAttribute(),
			// Use MU settings for auto adding properties & objects
			MakeShared<ConcertClientSharedSlate::FStreamExtenderBySettings>(
				TAttribute<const FConcertReplicationEditorSettings*>::CreateLambda([]()
				{
					return &UMultiUserReplicationSettings::Get()->ReplicationEditorSettings;
				}))
			))
		, LocalClientStreamDiffer(
			GetStreamSynchronizer(),
			ClientContentStorage->Stream->MakeReplicationMapGetterAttribute(),
			FStreamChangeTracker::FOnModifyReplicationMap::CreateLambda([this](){ ClientContentStorage->Stream->Modify(); })
			)
		, LocalAuthorityDiffer(EndpointId, *AuthoritySynchronizer, InAuthorityCache)
		, ChangeRequestBuilder(EndpointId, InAuthorityCache, *StreamSynchronizer, LocalClientStreamDiffer, LocalAuthorityDiffer)
		, AutoSubmissionPolicy(SubmissionQueue, ChangeRequestBuilder, LocalClientEditModel.Get(), LocalAuthorityDiffer)
	{
		LocalClientEditModel->OnObjectsChanged().AddRaw(this, &FReplicationClient::OnObjectsChanged);
		LocalClientEditModel->OnPropertiesChanged().AddRaw(this, &FReplicationClient::OnPropertiesChanged);
		LocalAuthorityDiffer.OnAddedOwnedObjects().AddRaw(this, &FReplicationClient::DeferOnModelChanged);

		SubmissionWorkflow->OnAuthorityRequestCompleted_AnyThread().AddRaw(this, &FReplicationClient::OnAuthoritySubmissionCompleted);
		StreamSynchronizer->OnServerStateChanged().AddRaw(this, &FReplicationClient::OnServerStateChanged);
	}

	FReplicationClient::~FReplicationClient()
	{
		FCoreDelegates::OnEndFrame.RemoveAll(this);
	}

	bool FReplicationClient::AllowsEditing() const
	{
		return CanEverSubmit(SubmissionWorkflow->GetUploadability()); 
	}

	void FReplicationClient::OnObjectsChanged(
		TConstArrayView<UObject*> AddedObjects,
		TConstArrayView<FSoftObjectPath> RemovedObjects,
		ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason
		)
	{
		DeferOnModelChanged(AddedObjects);
	}

	void FReplicationClient::OnPropertiesChanged()
	{
		DeferOnModelChanged();
	}

	void FReplicationClient::OnServerStateChanged()
	{
		// Whenever this client's server state changes, the UI must be refreshed.
		GetClientContent()->Stream->ReplicationMap = GetStreamSynchronizer().GetServerState();

		DeferOnModelChanged();
	}

	void FReplicationClient::DeferOnModelChanged(TConstArrayView<UObject*> AddedObjects)
	{
		if (!DeferredOnModelChangedData)
		{
			DeferredOnModelChangedData.Emplace();
			FCoreDelegates::OnEndFrame.AddRaw(this, &FReplicationClient::ProcessOnModelChanged);
		}

		Algo::Transform(AddedObjects, DeferredOnModelChangedData->AccumulatedAddedObjects, [](UObject* Object)
		{
			return Object;
		});
	}

	void FReplicationClient::ProcessOnModelChanged()
	{
		check(DeferredOnModelChangedData);
		const FDeferredOnModelChangedData ChangeData = MoveTemp(*DeferredOnModelChangedData);
		DeferredOnModelChangedData.Reset();
		FCoreDelegates::OnEndFrame.RemoveAll(this);
		
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		// This must be done before SetAuthorityIfAllowed because it uses the cache for checking whether the object has properties assigned
		LocalClientStreamDiffer.RefreshChangesCache();
		
		// Better UX for user: automatically take authority for newly added objects (but only if it is allowed and causes no conflicts)
		TakeAuthorityOverNewlyAddedObjects(ChangeData);
		// Refresh because local authority changes may no longer be valid after modifying the stream
		LocalAuthorityDiffer.RefreshChanges();

		// Finally, let everybody else know.
		OnModelChangedDelegate.Broadcast();
		AutoSubmissionPolicy.ProcessAccumulatedChangesAndSubmit();
	}
	
	void FReplicationClient::TakeAuthorityOverNewlyAddedObjects(const FDeferredOnModelChangedData& ChangeData)
	{
		TArray<FSoftObjectPath> ObjectPaths;
		Algo::TransformIf(ChangeData.AccumulatedAddedObjects, ObjectPaths,
			[](const TWeakObjectPtr<UObject>& Object)
			{
				// The object might have been made invalid last frame.
				return Object.IsValid();
			},
			[](const TWeakObjectPtr<UObject>& Object)
			{
				return FSoftObjectPath(Object.Get()) ;
			});
		LocalAuthorityDiffer.SetAuthorityIfAllowed(ObjectPaths, true);
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
