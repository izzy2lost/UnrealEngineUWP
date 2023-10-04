// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SSubobjectEditor.h"

namespace UE::ConcertClientSharedSlate
{
	/** Extends SSubobjectEditor to display components on the actor selected in the replication outliner. */
	class SConcertReplicationSubobjectEditor : public SSubobjectEditor
	{
	public:

		DECLARE_DELEGATE(FOnSubobjectsSelected);
		
		SLATE_BEGIN_ARGS(SConcertReplicationSubobjectEditor)
		{}
			/** Delegate to invoke on selection update. */
			SLATE_EVENT(FOnSubobjectsSelected, OnSubobjectsSelected)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Sets the object to display subjects for */
		void SetDisplayedRootObject(UObject* Object = nullptr);
		/** Gets the objects currently selected. */
		TSet<const UObject*> GetSelectedObjects() const { return GetSelectedObjectsFromNodes(GetSelectedNodes()); }

		//~ Begin SSubobjectEditor Interface
		virtual void OnAttachToDropAction(FSubobjectEditorTreeNodePtrType DroppedOn,const TArray<FSubobjectEditorTreeNodePtrType>& DroppedNodePtrs) override {}
		virtual void OnDetachFromDropAction(const TArray<FSubobjectEditorTreeNodePtrType>& DroppedNodePtrs) override {}
		virtual void OnMakeNewRootDropAction(FSubobjectEditorTreeNodePtrType DroppedNodePtr) override {}
		virtual void PostDragDropAction(bool bRegenerateTreeNodes) override {}
		virtual TSharedPtr<SWidget> BuildSceneRootDropActionMenu(FSubobjectEditorTreeNodePtrType DroppedOntoNodePtr, FSubobjectEditorTreeNodePtrType DroppedNodePtr) override;
		virtual void OnDeleteNodes() override {}
		//~ End SSubobjectEditor Interface

	protected:

		//~ Begin SSubobjectEditor Interface
		virtual void CopySelectedNodes() override {}
		virtual void PasteNodes() override {}
		virtual void OnDuplicateComponent() override {}
		virtual void PopulateContextMenuImpl(UToolMenu* InMenu, TArray<FSubobjectEditorTreeNodePtrType>& InSelectedItems, bool bIsChildActorSubtreeNodeSelected) override {}
		virtual FSubobjectDataHandle AddNewSubobject(const FSubobjectDataHandle& ParentHandle, UClass* NewClass, UObject* AssetOverride, FText& OutFailReason, TUniquePtr<FScopedTransaction> InOngoingTransaction) override;
		//~ End SSubobjectEditor Interface
		
	private:
		
		/** The object the subobject hierarchy is displayed for */
		TWeakObjectPtr<UObject> DisplayedRootObject;

		/** Delegate to invoke on selection update. */
		FOnSubobjectsSelected OnSubobjectsSelectedDelegate;
		
		/** @return The object to display the subobject hierarchy for. */
		UObject* GetContextObject() const;

		/** Utils for converting node selection to objects. */
		static TSet<const UObject*> GetSelectedObjectsFromNodes(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes);

		/** Called when the selection is updated */
		void HandleSelectionUpdated(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes) const;
	};
}

