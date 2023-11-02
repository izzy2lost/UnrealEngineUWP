// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransactionalPropertySelectionModel.h"
#include "ScopedTransaction.h"
#include "Misc/TransactionObjectEvent.h"

#define LOCTEXT_NAMESPACE "FTransactionalPropertySelectionModel"

namespace UE::ConcertClientSharedSlate
{
	FTransactionalPropertySelectionModel::FTransactionalPropertySelectionModel(
		UObject& OwningObject,
		TAttribute<FObjectReplicationMap*> ReplicationMapAttribute
		)
		: FGenericPropertySelectionModel(MoveTemp(ReplicationMapAttribute))
		, OwningObject(&OwningObject)
	{}

	void FTransactionalPropertySelectionModel::AddObjects(TConstArrayView<UObject*> Objects)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddObjects", "Add replicated objects"));
		OwningObject->Modify();
		FGenericPropertySelectionModel::AddObjects(Objects);
	}

	void FTransactionalPropertySelectionModel::RemoveObjects(TConstArrayView<FSoftObjectPath> Objects)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveObjects", "Remove replicated objects"));
		OwningObject->Modify();
		FGenericPropertySelectionModel::RemoveObjects(Objects);
	}

	void FTransactionalPropertySelectionModel::AddProperties(const FSoftObjectPath& SoftObjectPath, TConstArrayView<FConcertPropertyChain> Properties)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddProperties", "Add replicated properties"));
		OwningObject->Modify();
		FGenericPropertySelectionModel::AddProperties(SoftObjectPath, Properties);
	}

	void FTransactionalPropertySelectionModel::RemoveProperties(const FSoftObjectPath& SoftObjectPath, TConstArrayView<FConcertPropertyChain> Properties)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveProperties", "Remove replicated properties"));
		OwningObject->Modify();
		FGenericPropertySelectionModel::RemoveProperties(SoftObjectPath, Properties);
	}

	bool FTransactionalPropertySelectionModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		if (!OwningObject.IsValid())
		{
			return false;
		}
		
		for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
		{
			UObject* Object = TransactionObjectPair.Key;
			while (Object != nullptr)
			{
				if (Object == OwningObject)
				{
					return true;
				}
				Object = Object->GetOuter();
			}
		}

		return false;
	}

	void FTransactionalPropertySelectionModel::PostUndo(bool bSuccess)
	{
		OnObjectsChanged().Broadcast({}, {}, EReplicatedObjectChangeReason::Transacted);
		OnPropertiesChanged().Broadcast();
	}

	void FTransactionalPropertySelectionModel::PostRedo(bool bSuccess)
	{
		OnObjectsChanged().Broadcast({}, {}, EReplicatedObjectChangeReason::Transacted);
		OnPropertiesChanged().Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE