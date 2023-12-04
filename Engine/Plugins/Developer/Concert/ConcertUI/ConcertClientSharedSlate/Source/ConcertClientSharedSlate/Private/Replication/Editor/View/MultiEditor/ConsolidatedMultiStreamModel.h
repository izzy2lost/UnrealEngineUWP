// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "UObject/GCObject.h"

class UReplicationStreamObject;

namespace UE::ConcertClientSharedSlate
{
	class IEditableMultiReplicationStreamModel;
}

namespace UE::ConcertClientSharedSlate
{
	/**
	 * A mock object that implements functions in a specific way to allow editing of multiple streams in a single SBaseReplicationStreamEditor UI.
	 * Coupled to SMultiReplicationStreamEditor.
	 *
	 * Function summary:
	 * - AddObject & RemoveObject calls are forwarded to WrittenToStream
	 * - Object getters, like ForEachReplicatedObject, query WrittenToStream AND the multiple streams in MultiStreamModel
	 * - Property editing is disabled
	 * 
	 * SMultiReplicationStreamEditor's responsibility is to consolidate all replicated objects in a single UI and display all properties in the class.
	 * The creator of SMultiReplicationStreamEditor is supposed to inject custom columns into the property section for assigning specific properties
	 * to the streams in the IEditableReplicationStreamModel. 
	 */
	class FConsolidatedMultiStreamModel
		: public IEditableReplicationStreamModel
		, public FGCObject
	{
	public:

		FConsolidatedMultiStreamModel(
			TSharedRef<IEditableMultiReplicationStreamModel> InMultiStreamModel,
			bool bShouldSupportTransactions,
			FGetAutoAssignTarget InGetAutoAssignTargetDelegate
			);
		virtual ~FConsolidatedMultiStreamModel();
		
		// The functions related to properties are checkNoEntry() because SBaseReplicationStreamEditor's FFakeObjectToPropertiesEditorModel masks those functions 
		
		//~ Begin IReplicationStreamModel Interface
		virtual FSoftClassPath GetObjectClass(const FSoftObjectPath& Object) const override;
		virtual bool ContainsObjects(const TSet<FSoftObjectPath>& Objects) const override;
		virtual bool ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const override { checkNoEntry(); return false; }
		virtual bool ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const override;
		virtual bool ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const override { checkNoEntry(); return false; }
		//~ End IReplicationStreamModel Interface

		//~ Begin IEditableReplicationStreamModel Interface
		virtual void AddObjects(TConstArrayView<UObject*> Objects) override;
		virtual void RemoveObjects(TConstArrayView<FSoftObjectPath> Objects) override;
		virtual void AddProperties(const FSoftObjectPath& Object, TConstArrayView<FConcertPropertyChain> Properties) override { checkNoEntry(); }
		virtual void RemoveProperties(const FSoftObjectPath& Object, TConstArrayView<FConcertPropertyChain> Properties) override { checkNoEntry(); }
		virtual FOnObjectsChanged& OnObjectsChanged() override { return OnObjectsChangedDelegate; }
		virtual FOnPropertiesChanged& OnPropertiesChanged() override { return OnPropertiesChangedDelegate; }
		//~ End IEditableReplicationStreamModel Interface

		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FConsolidatedMultiStreamModel"); }
		//~ End FGCObject Interface

	private:

		/** What StreamForAdding is editing. Optionally transacted. */
		TObjectPtr<UReplicationStreamObject> StreamForAddingObject;
		/**
		 * AddObjects calls are redirected to this model.
		 * That allows objects added via the add button to show up in the UI regardless of what the real streams contain.
		 *
		 * RemoveObjects are applied to both StreamForAdding and to the editable streams in MultiStreamModel.
		 */
		TSharedRef<IEditableReplicationStreamModel> StreamForAdding;

		/** Contains all the streams that are being displayed */
		TSharedRef<IEditableMultiReplicationStreamModel> MultiStreamModel;

		/** Called the equivalent event of WrittenToStream or any of the streams in MultiStreamModel is broadcast. */
		FOnObjectsChanged OnObjectsChangedDelegate;
		/** Never actually called, just to implement OnPropertiesChanged. */
		FOnPropertiesChanged OnPropertiesChangedDelegate;
		/** If set, also adds newly added objects to the target stream. */
		FGetAutoAssignTarget GetAutoAssignTargetDelegate;
		
		/** All streams we've subscribed to for changes. Used so we can unsubscribe when MultiStreamModel->OnStreamsChanged triggers. */
		TArray<TWeakPtr<IEditableReplicationStreamModel>> SubscribedToViewers;

		// Trigger our delegates when any of the substreams change.
		void OnObjectsChanged_StreamForAdding(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason);
		void OnObjectsChanged_ClientStream(TConstArrayView<UObject*> AddedObjects, TConstArrayView<FSoftObjectPath> RemovedObjects, EReplicatedObjectChangeReason ChangeReason);
		
		/** Trigger subscription rebuild when streams are added / removed. */
		void OnStreamExternallyChanged(TSharedRef<IReplicationStreamModel> Stream);
		void RebuildStreamSubscriptions();
		void ClearStreamSubscriptions();
	};
}

