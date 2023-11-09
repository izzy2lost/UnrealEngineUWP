// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Misc/Attribute.h"
#include "UObject/WeakObjectPtrTemplates.h"

struct FConcertReplicationEditorSettings;
class FTransactionObjectEvent;
struct FObjectReplicationMap;
struct FTransactionContext;

namespace UE::ConcertClientSharedSlate
{
	/** Implements logic for editing a FObjectReplicationMap contained in an UObject. */
	class FGenericPropertySelectionModel
		: public IEditableReplicationStreamModel
	{
	public:
		
		FGenericPropertySelectionModel(TAttribute<FObjectReplicationMap*> ReplicationMapAttribute);
		
		//~ Begin IReplicationStreamModel Interface
		virtual uint32 GetNumReplicatedObjects() const override;
		virtual uint32 GetNumProperties(const FSoftObjectPath& Object) const override;
		virtual FSoftClassPath GetObjectClass(const FSoftObjectPath& Object) const override;
		virtual bool ContainsObjects(const TSet<FSoftObjectPath>& Objects) const override;
		virtual bool ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const override;
		virtual bool ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const override;
		virtual bool ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Parent)> Delegate) const override;
		//~ End IReplicationStreamModel Interface
		
		//~ Begin IEditableReplicationStreamModel Interface
		virtual void AddObjects(TConstArrayView<UObject*> Objects) override;
		virtual void RemoveObjects(TConstArrayView<FSoftObjectPath> Objects) override;
		virtual void AddProperties(const FSoftObjectPath&, TConstArrayView<FConcertPropertyChain> Properties) override;
		virtual void RemoveProperties(const FSoftObjectPath&, TConstArrayView<FConcertPropertyChain> Properties) override;
		virtual FOnObjectsChanged& OnObjectsChanged() override { return OnObjectsChangedDelegate; }
		virtual FOnPropertiesChanged& OnPropertiesChanged() override { return OnPropertiesChangedDelegate; }
		//~ End IEditableReplicationStreamModel Interface

	private:

		/** Returns the replication map that is supposed to be edited. */
		TAttribute<FObjectReplicationMap*> ReplicationMapAttribute;

		FOnObjectsChanged OnObjectsChangedDelegate;
		FOnPropertiesChanged OnPropertiesChangedDelegate;
	};
}

