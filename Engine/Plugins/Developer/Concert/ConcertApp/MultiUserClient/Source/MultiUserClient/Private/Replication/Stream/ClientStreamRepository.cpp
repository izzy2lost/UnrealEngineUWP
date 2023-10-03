// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClientStreamRepository.h"

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "LocalClientStreamSynchronizer.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/IEditableObjectToPropertiesModel.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace UE::MultiUserClient
{
	FClientStreamRepository::FClientStreamRepository(TSharedRef<IConcertSyncClient> InClient)
		: SessionContent(NewObject<UMultiUserReplicationSessionPreset>(GetTransientPackage(), NAME_None, RF_Transient))
		, LocalClientContent(SessionContent->AddClient())
		, LocalClientEditModel(ConcertClientSharedSlate::CreatePropertySelectionModel(*LocalClientContent->Stream, LocalClientContent->Stream->MakeReplicationMapGetterAttribute()))
		, LocalClientStreamDiffer(MakeShared<FLocalClientStreamSynchronizer>(
			MoveTemp(InClient),
			LocalClientContent->Stream->StreamId,
			LocalClientContent->Stream->MakeReplicationMapGetterAttribute(),
			FLocalClientStreamSynchronizer::FOnModifyReplicationMap::CreateLambda([this](){ LocalClientContent->Stream->Modify(); })
			))
	{
		LocalClientEditModel->OnObjectsChanged().AddRaw(this, &FClientStreamRepository::OnObjectsChanged);
		LocalClientEditModel->OnPropertiesChanged().AddRaw(this, &FClientStreamRepository::OnPropertiesChanged);
		
		LocalClientStreamDiffer->OnChangesReverted_GameThread().AddLambda([this]()
		{
			OnModelChangedDelegate.Broadcast();
		});
	}

	void FClientStreamRepository::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(SessionContent);
		Collector.AddReferencedObject(LocalClientContent);
	}

	void FClientStreamRepository::OnObjectsChanged(
		TArrayView<UObject*> Objects,
		TArrayView<FSoftObjectPath> SoftObjectPaths,
		ConcertClientSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason
		)
	{
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		LocalClientStreamDiffer->RefreshChangesCache();
	}

	void FClientStreamRepository::OnPropertiesChanged()
	{
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		LocalClientStreamDiffer->RefreshChangesCache();
	}
}
