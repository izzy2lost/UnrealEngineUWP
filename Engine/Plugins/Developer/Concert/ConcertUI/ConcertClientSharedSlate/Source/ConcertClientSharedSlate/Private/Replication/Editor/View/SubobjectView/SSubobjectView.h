// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/IReplicationSubobjectView.h"
#include "Replication/Editor/View/PredefinedReplicationColumns.h"
#include "Replication/Editor/View/ObjectViewer/Tree/SReplicationTreeView.h"

namespace UE::ConcertClientSharedSlate
{
	class ISubobjectModel;
	class FReplicatedSubobjectData;
	class SConcertReplicationSubobjectEditor;

	/** Adapts SSubobjectEditor to the IReplicationSubobjectView interface. */
	class SSubobjectView : public IReplicationSubobjectView
	{
	public:

		SLATE_BEGIN_ARGS(SSubobjectView)
		{}
			/** Additional columns that should be displayed for the subobjects. */
			SLATE_ARGUMENT(TArray<ReplicationColumns::FReplicationSubobjectObjectColumn>, AdditionalColumns)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<ISubobjectModel> InSubobjectModel);

		//~ Begin IReplicationSubobjectView Interface
		virtual void SetTopLevelObjects(const TArray<FSoftObjectPath>& RootObjects) override;
		virtual void SelectTopLevelObjects() override;
		virtual TArray<FSoftObjectPath> GetSelectedObjects() const override;
		virtual FOnSelectionChanged& OnSelectionChanged() override { return OnSelectionChangedDelegate; }
		//~ End IReplicationSubobjectView Interface

	private:

		/** Gets the subobjects to display */
		TSharedPtr<ISubobjectModel> SubobjectModel;

		/** Reuses the SSubobjectEditor for displaying subobjects. */
		TSharedPtr<SReplicationTreeView<FReplicatedSubobjectData>> SubobjectTreeView;

		/** Contains all subobject data. */
		TArray<TSharedPtr<FReplicatedSubobjectData>> AllSubobjectData;
		/**
		 * Contains only the root elements of the tree, which are:
		 * - Item corresponding to top level object
		 * - Separators
		 * - Those objects returned by ISubobjectModel::ForEachRootSubobject
		 */
		TArray<TSharedPtr<FReplicatedSubobjectData>> RootSubobjectData;
		/** Inverse map of ObjectRowData using FReplicatedObjectData::GetObjectPath as key. Contains all elements of ObjectRowData. */
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedSubobjectData>> PathToObjectDataCache;

		/** Executes when the selection is changed. */
		FOnSelectionChanged OnSelectionChangedDelegate;

		void RefreshView();
		void BuildRootObjectData(TMap<FSoftObjectPath, TSharedPtr<FReplicatedSubobjectData>> NewPathToObjectDataCache);
		void BuildChildObjectData(TMap<FSoftObjectPath, TSharedPtr<FReplicatedSubobjectData>>& NewPathToObjectDataCache);
		
		void GetObjectRowChildren(TSharedPtr<FReplicatedSubobjectData> Item, TFunctionRef<void(TSharedPtr<FReplicatedSubobjectData>)> Callback) const;
		/** Returns a separator bar if the item is a separator */
		TSharedPtr<SWidget> GetOptionalColumnOverrideWidget(const FName& Name, const FReplicatedSubobjectData& RowData) const;
	};
}

