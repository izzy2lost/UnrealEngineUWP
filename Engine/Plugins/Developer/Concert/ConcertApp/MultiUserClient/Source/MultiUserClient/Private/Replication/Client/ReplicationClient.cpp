// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Stream/StreamChangeTracker.h"

namespace UE::MultiUserClient
{
	FReplicationClient::FReplicationClient(
		const FGuid& EndpointId,
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
		, LocalAuthorityDiffer(*AuthoritySynchronizer)
		, SubmissionWorkflow(MakeSubmissionWorkflowFunc(LocalClientStreamDiffer, LocalAuthorityDiffer, *StreamSynchronizer.Get()))
		, AutoSubmissionPolicy(*SubmissionWorkflow.Get(), LocalClientEditModel.Get(), LocalAuthorityDiffer)
	{
		LocalClientEditModel->OnObjectsChanged().AddRaw(this, &FReplicationClient::OnObjectsChanged);
		LocalClientEditModel->OnPropertiesChanged().AddRaw(this, &FReplicationClient::OnPropertiesChanged);
		
		LocalClientStreamDiffer.OnChangesReverted_GameThread().AddLambda([this]()
		{
			OnModelExternallyChangedDelegate.Broadcast();
		});
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
		
		// Better UX for user: automatically take authority for newly added objects
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
}
