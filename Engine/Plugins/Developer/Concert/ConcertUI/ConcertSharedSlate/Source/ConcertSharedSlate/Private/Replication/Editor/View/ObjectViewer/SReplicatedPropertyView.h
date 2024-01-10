// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/Editor/View/SelectionViewerColumns.h"

#include "Misc/Optional.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

namespace UE::ConcertSharedSlate
{
	class SPropertyTreeView;

	/** Displays the SPropertyTreeView and decorates it with messages that prompt the user for action, e.g. to select an object to view properties. */
	class SReplicatedPropertyView : public SCompoundWidget
	{
	public:
		
		DECLARE_DELEGATE_RetVal(TArray<TSharedPtr<FReplicatedObjectData>>, FGetSelectedRootObjects)

		SLATE_BEGIN_ARGS(SReplicatedPropertyView)
		{}
			/** Additional columns to add to the property view */
			SLATE_ARGUMENT(TArray<ReplicationColumns::FReplicationPropertyColumn>, AdditionalPropertyColumns)
			/** Initial primary sort to set. */
			SLATE_ARGUMENT(FColumnSortInfo, PrimarySort)
			/** Initial secondary sort to set. Defaults to Label column. */
			SLATE_ARGUMENT(FColumnSortInfo, SecondarySort)
		
			/** Gets the root objects selected in the object outliner. */
			SLATE_EVENT(FGetSelectedRootObjects, GetSelectedRootObjects)
		
			/** Optional widget to add to the left of the property list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfPropertySearchBar)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel);
		
		void RefreshPropertyData();
		/** Requests that the given column be resorted, if it currently affects the row sorting. */
		void RequestResortForColumn(const FName& ColumnId);
		
		const TArray<TSharedPtr<FReplicatedPropertyData>>& GetPropertyRowData() const { return PropertyRowData; }
		TArray<FSoftObjectPath> GetObjectsSelectedForPropertyEditing() const;

	private:
		
		/** The model this view is visualizing. */
		TSharedPtr<IReplicationStreamModel> PropertiesModel;
		
		/** Tree view for replicated properties. Content depends on the current object selected. */
		TSharedPtr<SPropertyTreeView> ReplicatedProperties;
		
		enum class EReplicatedPropertyContent
		{
			/** Shows the properties */
			Properties,
			/** Prompts: "Select an object to see selected properties" */
			NoSelection,
			/** Prompts: "Select objects of the same type type to see selected properties" */
			SelectionTooBig
		};
		/** Determines the content displayed for PropertyArea. */
		TSharedPtr<SWidgetSwitcher> PropertyContent;
		
		/** Gets the root objects selected in the object outliner. */
		FGetSelectedRootObjects GetSelectedRootObjectsDelegate;
		
		/**
		 * These instances can be subclasses of FReplicatedPropertyData, e.g. FReplicatedPropertyData_Editor.
		 * Their type can be overridden by subclasses.
		 * They only have the FReplicatedPropertyData type so they can be passed efficiently to SObjectToPropertyView.
		 * @see GetPropertyData
		 */
		TArray<TSharedPtr<FReplicatedPropertyData>> PropertyRowData;
		/** The instances of ObjectRowData which do not have any parents. This acts as the item source for the tree view. */
		TArray<TSharedPtr<FReplicatedPropertyData>> RootPropertyRowData;
		/** Inverse map of PropertyRowData using FReplicatedPropertyData::GetProperty as key. Contains all elements of PropertyRowData. */
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> ChainToPropertyDataCache;

		TSharedRef<SWidget> CreatePropertiesView(const FArguments& InArgs);
		
		TSharedRef<FReplicatedPropertyData> AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain);

		/** Inits RootPropertyRowData from PropertyRowData. */
		void BuildRootPropertyRowData();
		/** Given the selected objects, determines whether they all have the same class and returns it if so. */
		TOptional<FSoftClassPath> GetClassForPropertiesFromSelection(const TArray<FSoftObjectPath>& Objects) const;
		void GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild);
		
		// Utils
		void SetPropertyContent(EReplicatedPropertyContent Content) const;
	};
}


