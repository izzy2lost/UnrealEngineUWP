// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/Editor/View/SelectionViewerColumns.h"

#include "Misc/Optional.h"
#include "Property/SFilteredPropertyTreeView.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

namespace UE::ConcertSharedSlate
{
	class SFilteredPropertyTreeView;

	/**
	 * Determines which properties are to be displayed based on an IReplicationStreamModel.
	 * Uses a property tree for displaying. If no properties are displayed, this widget displays a message instead, e.g. to select an object.
	 */
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
		
			/** Optional. If set, this determines the display text for objects. */
			SLATE_ARGUMENT(TSharedPtr<IObjectNameModel>, NameModel)
		
			/** Optional widget to add to the left of the property list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfPropertySearchBar)
			/** Optional widget to add to the right of the property list search bar. */
			SLATE_NAMED_SLOT(FArguments, RightOfPropertySearchBar)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel);
		
		void RefreshPropertyData();
		/** Requests that the given column be resorted, if it currently affects the row sorting. */
		void RequestResortForColumn(const FName& ColumnId) const { ReplicatedProperties->RequestResortForColumn(ColumnId); }
		
		TArray<FSoftObjectPath> GetObjectsSelectedForPropertyEditing() const;

	private:
		
		/** Tree view for replicated properties. Content depends on the current object selected. */
		TSharedPtr<SFilteredPropertyTreeView> ReplicatedProperties;
		
		/** The model this view is visualizing. */
		TSharedPtr<IReplicationStreamModel> PropertiesModel;
		
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

		/** Used to determine whether to rebuild the entire property data. */
		TArray<FSoftObjectPath> PreviousSelectedObjects;

		TSharedRef<SWidget> CreatePropertiesView(const FArguments& InArgs);
		
		/** Given the selected objects, determines whether they all have the same class and returns it if so. */
		TOptional<FSoftClassPath> GetClassForPropertiesFromSelection(const TArray<FSoftObjectPath>& Objects) const;
		/** Sets how to display this widget */
		void SetPropertyContent(EReplicatedPropertyContent Content) const;
	};
}


