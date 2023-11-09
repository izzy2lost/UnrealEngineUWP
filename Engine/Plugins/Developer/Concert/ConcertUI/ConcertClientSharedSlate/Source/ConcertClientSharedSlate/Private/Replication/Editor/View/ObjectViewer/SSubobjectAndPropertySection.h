// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/ObjectViewer/Tree/SelectionViewerColumns.h"
#include "Replication/ReplicationWidgetDelegates.h"

#include "Misc/Optional.h"
#include "Replication/Data/ConcertPropertySelection.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SWidgetSwitcher;

namespace UE::ConcertClientSharedSlate
{
	class SReplicatedPropertiesView;
	class IReplicationSubobjectView;

	/**
	 * Displays an optional IReplicationSubobjectView and followed by SReplicatedPropertiesView tree view (which is always created).
	 * 
	 * Important: this view should be possible to be built in programs, so it should not reference things like AActor,
	 * UActorComponent, ResolveObject, etc. directly. 
	 */
	class SSubobjectAndPropertySection : public SCompoundWidget
	{
	public:
		
		DECLARE_DELEGATE_RetVal(TArray<TSharedPtr<FReplicatedObjectData>>, FGetSelectedRootObjects)

		SLATE_BEGIN_ARGS(SSubobjectAndPropertySection)
		{}
			/** Additional columns to add to the property view */
			SLATE_ARGUMENT(TArray<ReplicationColumns::FReplicationPropertyColumn>, AdditionalPropertyColumns)
			/** Optional. Placed between root object outliner and property editor. */
			SLATE_ARGUMENT(TSharedPtr<IReplicationSubobjectView>, SubobjectView)
		
			/** Optional. Used for determining the order in which properties are displayed. */
			SLATE_EVENT(FSortPropertyPredicate, SortPropertyRowPredicate)
			/** Gets the root objects selected in the object outliner. */
			SLATE_EVENT(FGetSelectedRootObjects, GetSelectedRootObjects)
		
			/** Optional widget to add to the left of the property list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfPropertySearchBar)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IReplicationStreamModel> InPropertiesModel);

		/** If there is a subobject view, makes it select the root objects. */
		void SelectRootObjects() const;
		/** Clears the subobject selection */
		void ClearSubobjectSelection() const;
		
		void RefreshSubobjectData();
		void RefreshPropertyData();
		
		const TArray<TSharedPtr<FReplicatedPropertyData>>& GetPropertyRowData() const { return PropertyRowData; }
		TArray<FSoftObjectPath> GetObjectsSelectedForPropertyEditing() const;

	private:
		
		/** The model this view is visualizing. */
		TSharedPtr<IReplicationStreamModel> PropertiesModel;
		
		/** Optional. External widget that selects subobjects from ActorArea. */
		TSharedPtr<IReplicationSubobjectView> SubobjectView;
		/** Tree view for replicated properties. Content depends on the current object selected. */
		TSharedPtr<SReplicatedPropertiesView> ReplicatedProperties;
		
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
		
		/** Used for determining the order in which properties are displayed. Default: Sort by name. */
		FSortPropertyPredicate SortPropertyRowPredicate;
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

		TSharedRef<SWidget> CreateSubobjectsAndPropertiesSection(const FArguments& InArgs);
		TSharedRef<SWidget> CreatePropertiesView(const FArguments& InArgs);
		
		TSharedRef<FReplicatedPropertyData> AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain);

		/** Inits RootPropertyRowData from PropertyRowData. */
		void BuildRootPropertyRowData();
		/** Given the selected objects, determines whether they all have the same class and returns it if so. */
		TOptional<FSoftClassPath> GetClassForPropertiesFromSelection(const TArray<FSoftObjectPath>& Objects) const;
		void GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild);
		
		void OnSubobjectSelectionChanged();
		
		// Utils
		void SortPropertyRowArray(TArray<TSharedPtr<FReplicatedPropertyData>>& ToSort) const;
		void SetPropertyContent(EReplicatedPropertyContent Content) const;
	};
}


