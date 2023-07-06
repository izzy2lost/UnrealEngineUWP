// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "IEditableObjectToPropertiesModel.h"
#include "MultiUserPropertyReplicationSelection.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FTransactionObjectEvent;
struct FTransactionContext;

namespace UE::MultiUserReplicationEditor
{
	/** Edits a UMultiUserPropertyReplicationSelection object. */
	class FPropertySelectionAssetModel
		: public IEditableObjectToPropertiesModel
		, public FSelfRegisteringEditorUndoClient
	{
	public:
		
		FPropertySelectionAssetModel(UMultiUserPropertyReplicationSelection& Object);
		
		//~ Begin IObjectToPropertiesModel Interface
		virtual uint32 GetNumReplicatedObjects() const override;
		virtual uint32 GetNumProperties(const FSoftObjectPath& Object) const override;
		virtual FSoftClassPath GetObjectClass(const FSoftObjectPath& Object) const override;
		virtual bool ContainsObjects(const TSet<FSoftObjectPath>& Objects) const override;
		virtual bool ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const override;
		virtual bool ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const override;
		virtual bool ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Parent)> Delegate) const override;
		//~ End IObjectToPropertiesModel Interface
		
		//~ Begin IEditableObjectToPropertiesModel Interface
		virtual void AddObjects(TArrayView<UObject*> Objects) override;
		virtual void RemoveObjects(TArrayView<FSoftObjectPath> Objects) override;
		virtual void AddProperties(const FSoftObjectPath&, TArrayView<FConcertPropertyChain> Properties) override;
		virtual void RemoveProperties(const FSoftObjectPath&, TArrayView<FConcertPropertyChain> Properties) override;
		virtual FOnObjectsChanged& OnObjectsChanged() override { return OnObjectsChangedDelegate; }
		virtual FOnPropertiesChanged& OnPropertiesChanged() override { return OnPropertiesChangedDelegate; }
		//~ End IEditableObjectToPropertiesModel Interface

		//~ Begin FEditorUndoClient Interface
		virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FEditorUndoClient Interface

	private:

		TWeakObjectPtr<UMultiUserPropertyReplicationSelection> Asset;

		FOnObjectsChanged OnObjectsChangedDelegate;
		FOnPropertiesChanged OnPropertiesChangedDelegate;
	};
}

