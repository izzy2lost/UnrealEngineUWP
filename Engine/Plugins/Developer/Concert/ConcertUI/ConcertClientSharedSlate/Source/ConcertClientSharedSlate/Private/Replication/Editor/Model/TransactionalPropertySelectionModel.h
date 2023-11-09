// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPropertySelectionModel.h"
#include "EditorUndoClient.h"

namespace UE::ConcertClientSharedSlate
{
	/** Special case of FGenericPropertySelectionModel where the edited FObjectReplicationMap lives in an UObject that is RF_Transactional. */
	class FTransactionalPropertySelectionModel
		: public FGenericPropertySelectionModel
		, public FSelfRegisteringEditorUndoClient
	{
	public:

		FTransactionalPropertySelectionModel(
			UObject& OwningObject,
			TAttribute<FObjectReplicationMap*> ReplicationMapAttribute
			);
		
		//~ Begin IEditableReplicationStreamModel Interface
		virtual void AddObjects(TConstArrayView<UObject*> Objects) override;
		virtual void RemoveObjects(TConstArrayView<FSoftObjectPath> Objects) override;
		virtual void AddProperties(const FSoftObjectPath&, TConstArrayView<FConcertPropertyChain> Properties) override;
		virtual void RemoveProperties(const FSoftObjectPath&, TConstArrayView<FConcertPropertyChain> Properties) override;
		//~ End IEditableReplicationStreamModel Interface

		//~ Begin FEditorUndoClient Interface
		virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FEditorUndoClient Interface

	private:

		/** User of FTransactionalPropertySelectionModel is responsible for keeping OwningObject alive, e.g. via an asset editor. */
		TWeakObjectPtr<UObject> OwningObject;
	};
}


