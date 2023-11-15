// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConsolidatedMultiStreamModel.h"

#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/ReplicationStreamObject.h"
#include "Replication/Editor/Model/TransactionalReplicationStreamModel.h"

#include "UObject/Package.h"

namespace UE::ConcertClientSharedSlate
{
	namespace Private
	{
		static UReplicationStreamObject* MakeObject(bool bShouldSupportTransactions)
		{
			const EObjectFlags Flags = RF_Transient | (bShouldSupportTransactions ? RF_Transactional : RF_NoFlags);
			return NewObject<UReplicationStreamObject>(GetTransientPackage(), NAME_None, Flags);
		}

		static TSharedRef<IEditableReplicationStreamModel> MakeModel(UReplicationStreamObject* Object, bool bShouldSupportTransactions)
		{
			TAttribute<FObjectReplicationMap*> Attribute = TAttribute<FObjectReplicationMap*>::CreateLambda([WeakPtr = TWeakObjectPtr<UReplicationStreamObject>(Object)]() -> FObjectReplicationMap* 
			{
				if (WeakPtr.IsValid())
				{
					return &WeakPtr->ReplicationMap;
				}
				return nullptr;
			});
			
			if (bShouldSupportTransactions)
			{
				return MakeShared<FTransactionalReplicationStreamModel>(*Object, MoveTemp(Attribute));
			}
			return MakeShared<FGenericReplicationStreamModel>(MoveTemp(Attribute));
		}
	}
	
	FConsolidatedMultiStreamModel::FConsolidatedMultiStreamModel(
		TSharedRef<IEditableMultiReplicationStreamModel> InMultiStreamModel,
		bool bShouldSupportTransactions
		)
		: StreamForAddingObject(Private::MakeObject(bShouldSupportTransactions))
		, StreamForAdding(Private::MakeModel(StreamForAddingObject, bShouldSupportTransactions))
		, MultiStreamModel(MoveTemp(InMultiStreamModel))
	{
		MultiStreamModel->OnReadOnlyStreamChanged().AddRaw(this, &FConsolidatedMultiStreamModel::OnReadOnlyStreamChanged);
		MultiStreamModel->OnStreamSetChanged().AddRaw(this, &FConsolidatedMultiStreamModel::RebuildStreamSubscriptions);
		StreamForAdding->OnObjectsChanged().AddRaw(this, &FConsolidatedMultiStreamModel::OnObjectsChanged_StreamForAdding);
		RebuildStreamSubscriptions();
	}

	FConsolidatedMultiStreamModel::~FConsolidatedMultiStreamModel()
	{
		MultiStreamModel->OnReadOnlyStreamChanged().RemoveAll(this);
		MultiStreamModel->OnStreamSetChanged().RemoveAll(this);
		StreamForAdding->OnObjectsChanged().RemoveAll(this);
		ClearStreamSubscriptions();
	}

	FSoftClassPath FConsolidatedMultiStreamModel::GetObjectClass(const FSoftObjectPath& Object) const
	{
		FSoftClassPath Result = StreamForAdding->GetObjectClass(Object);
		if (Result.IsValid())
		{
			return Result;
		}

		MultiStreamModel->ForEachStream([&Object, &Result](const TSharedRef<IReplicationStreamModel>& Stream)
        {
        	Result = Stream->GetObjectClass(Object);
        	return Result.IsValid() ? EBreakBehavior::Break : EBreakBehavior::Continue;
        });
		return Result;
	}

	bool FConsolidatedMultiStreamModel::ContainsObjects(const TSet<FSoftObjectPath>& Objects) const
	{
		if (StreamForAdding->ContainsObjects(Objects))
		{
			return true;
		}

		bool bContains = false;
		MultiStreamModel->ForEachStream([&Objects, &bContains](const TSharedRef<IReplicationStreamModel>& Stream)
		{
			bContains = Stream->ContainsObjects(Objects);
			return bContains ? EBreakBehavior::Break : EBreakBehavior::Continue;
		});
		return bContains;
	}		

	bool FConsolidatedMultiStreamModel::ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const
	{
		TSet<FSoftObjectPath> UniquePathsOnly;
		EBreakBehavior BreakBehavior = EBreakBehavior::Continue;
		const auto ProcessObjects = [&Delegate, &UniquePathsOnly, &BreakBehavior](const FSoftObjectPath& Object)
		{
			bool bAlreadyInSet = false;
			UniquePathsOnly.Add(Object, &bAlreadyInSet);
			if (!bAlreadyInSet)
			{
				BreakBehavior = Delegate(Object);
			}
			
			return BreakBehavior;
		};
		
		bool bAnyMappings = StreamForAdding->ForEachReplicatedObject(ProcessObjects);
		if (BreakBehavior == EBreakBehavior::Break)
		{
			return bAnyMappings;
		}

		MultiStreamModel->ForEachStream([&BreakBehavior, &ProcessObjects, &bAnyMappings](const TSharedRef<IReplicationStreamModel>& Stream)
		{
			bAnyMappings |= Stream->ForEachReplicatedObject(ProcessObjects);
			return BreakBehavior;
		});
		
		return bAnyMappings;
	}

	void FConsolidatedMultiStreamModel::AddObjects(TConstArrayView<UObject*> Objects)
	{
		StreamForAdding->AddObjects(Objects);
	}

	void FConsolidatedMultiStreamModel::RemoveObjects(TConstArrayView<FSoftObjectPath> Objects)
	{
		StreamForAdding->RemoveObjects(Objects);
		for (const TSharedRef<IEditableReplicationStreamModel>& Model : MultiStreamModel->GetEditableStreams())
		{
			Model->RemoveObjects(Objects);
		}
	}

	void FConsolidatedMultiStreamModel::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(StreamForAddingObject);
	}

	void FConsolidatedMultiStreamModel::OnObjectsChanged_StreamForAdding(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason)
	{
		OnObjectsChangedDelegate.Broadcast(AddedObjects, RemovedObjects, ChangeReason);
	}

	void FConsolidatedMultiStreamModel::OnObjectsChanged_ClientStream(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason)
	{
		OnObjectsChangedDelegate.Broadcast(AddedObjects, RemovedObjects, ChangeReason);
	}

	void FConsolidatedMultiStreamModel::OnReadOnlyStreamChanged(TSharedRef<IReplicationStreamModel> Stream)
	{
		OnObjectsChangedDelegate.Broadcast({}, {}, EReplicatedObjectChangeReason::ExternalChange);
	}

	void FConsolidatedMultiStreamModel::RebuildStreamSubscriptions()
	{
		ClearStreamSubscriptions();

		for (const TSharedRef<IEditableReplicationStreamModel>& Model : MultiStreamModel->GetEditableStreams())
		{
			Model->OnObjectsChanged().AddRaw(this, &FConsolidatedMultiStreamModel::OnObjectsChanged_ClientStream);
		}
	}

	void FConsolidatedMultiStreamModel::ClearStreamSubscriptions()
	{
		for (const TWeakPtr<IEditableReplicationStreamModel>& Model : SubscribedToViewers)
		{
			if (const TSharedPtr<IEditableReplicationStreamModel> ModelPin = Model.Pin())
			{
				ModelPin->OnObjectsChanged().RemoveAll(this);
			}
		}
		SubscribedToViewers.Empty();
	}
}
