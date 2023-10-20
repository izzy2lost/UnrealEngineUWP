// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Stream/StreamChangeTracker.h"

namespace UE::MultiUserClient
{
	FReplicationClient::FReplicationClient(
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TUniquePtr<IClientAuthoritySynchronizer> InAuthoritySynchronizer,
		TFunctionRef<TUniquePtr<ISubmissionWorkflow>()> MakeSubmissionWorkflowFunc
		)
		: ClientContentStorage(&InSessionContent)
		, StreamSynchronizer(MoveTemp(InStreamSynchronizer))
		, AuthoritySynchronizer(MoveTemp(InAuthoritySynchronizer))
		, LocalClientEditModel(ConcertClientSharedSlate::CreatePropertySelectionModel(*ClientContentStorage->Stream, ClientContentStorage->Stream->MakeReplicationMapGetterAttribute()))
		, LocalClientStreamDiffer(
			GetStreamSynchronizer(),
			ClientContentStorage->Stream->MakeReplicationMapGetterAttribute(),
			FStreamChangeTracker::FOnModifyReplicationMap::CreateLambda([this](){ ClientContentStorage->Stream->Modify(); })
			)
		, LocalAuthorityDiffer(*AuthoritySynchronizer)
	{
		SubmissionWorkflow = MakeSubmissionWorkflowFunc();
		
		LocalClientEditModel->OnObjectsChanged().AddRaw(this, &FReplicationClient::OnObjectsChanged);
		LocalClientEditModel->OnPropertiesChanged().AddRaw(this, &FReplicationClient::OnPropertiesChanged);
		
		LocalClientStreamDiffer.OnChangesReverted_GameThread().AddLambda([this]()
		{
			OnModelExternallyChangedDelegate.Broadcast();
		});
	}

	void FReplicationClient::OnObjectsChanged(
		TConstArrayView<UObject*> Objects,
		TConstArrayView<FSoftObjectPath> SoftObjectPaths,
		ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason
		)
	{
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		LocalClientStreamDiffer.RefreshChangesCache();
	}

	void FReplicationClient::OnPropertiesChanged()
	{
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		LocalClientStreamDiffer.RefreshChangesCache();
	}
}
