// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Data/ConcertPropertySelection.h"
#include "ReplicatedObjectData.h"
#include "SReplicationTreeView.h"
#include "StreamEditor/View/ObjectViewer/SelectionViewerColumns.h"

#include "Algo/Transform.h"
#include "Misc/Optional.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SExpandableArea;
class SWidgetSwitcher;
struct FSoftClassPath;
struct FSoftObjectPath;

namespace UE::MultiUserReplicationEditor
{
	class FReplicatedPropertyData;
	class FReplicatedObjectData;
	class IEditableObjectToPropertiesModel;
	class IObjectToPropertiesModel;

	DECLARE_DELEGATE(FOnPropertiesRefreshed);
	DECLARE_DELEGATE_RetVal_TwoParams(bool, FSortPropertyPredicate, const TSharedPtr<FReplicatedPropertyData>& Left, const TSharedPtr<FReplicatedPropertyData>& Right);
	
	/**
	 * Root widget for viewing UMultiUserPropertyReplicationSelection.
	 * This widget knows how to display IObjectToPropertiesModel.
	 * 
	 * The underlying data is modified by SPropertyReplicationSelectionEditor, which uses this widget's extension
	 * points to call functions on IEditableObjectToPropertiesModel.
	 */
	class MULTIUSERREPLICATIONEDITOR_API SPropertyReplicationSelectionViewer : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SPropertyReplicationSelectionViewer)
		{}
			/** Additional columns to add to the object view */
			SLATE_ARGUMENT(TArray<ReplicationObjectColumns::FReplicationObjectColumn>, AdditionalObjectColumns)
			/** Additional columns to add to the property view */
			SLATE_ARGUMENT(TArray<ReplicationPropertyColumns::FReplicationPropertyColumn>, AdditionalPropertyColumns)

			/** Optional. Called when the delete key is pressed in the object view. */
			SLATE_EVENT(SReplicationTreeView<TSharedPtr<FReplicatedObjectData>>::FDeleteItems, OnDeleteObjects)
		
			/** Called to generate the context menu for objects. */
			SLATE_EVENT(FOnContextMenuOpening, OnObjectsContextMenuOpening)

			/** Optional. Used for determining the order in which properties are displayed. */
			SLATE_EVENT(FSortPropertyPredicate, SortPropertyRowPredicate)
		
			/** Optional widget to add to the left of the object list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfObjectSearchBar)
			/** Optional widget to add to the left of the property list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfPropertySearchBar)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IObjectToPropertiesModel> InPropertiesModel);

		void RefreshObjectData();
		void RefreshPropertyData();

		/** Sets only these objects to be selected. */
		template<typename TObjectRangeType>
		void SetSelectedObjects(const TObjectRangeType& ObjectsToSelect, bool bIsSelected);
		/** Expands the given items. Does not unexpand already expanded items. */
		template<typename TObjectRangeType>
		void SetObjectsExpanded(const TObjectRangeType& ObjectsToSelect, bool bIsExpanded);

		/** Given the selected objects, determines whether they all have the same class and returns it if so. */
		TOptional<FSoftClassPath> GetClassForPropertiesFromSelection() const;

		/** Gets the child items of the given object. */
		template<typename TObjectType>
		void GetObjectChildren(const TObjectType& ReplicatedObjectData, TFunctionRef<void(const FSoftObjectPath&)> ProcessChild);
		/** @return The parent of the object associated with Path or null (check IsNull) if none exists. */
		FSoftObjectPath GetParent(const FSoftObjectPath& Path) const;

		const TSet<TSharedPtr<FReplicatedObjectData>>& GetObjectRowData() const { return ObjectRowData; }
		const TArray<TSharedPtr<FReplicatedPropertyData>>& GetPropertyRowData() const { return PropertyRowData; }

		TArray<TSharedPtr<FReplicatedObjectData>> GetSelectedObjects() const { return ReplicatedObjects->GetSelectedItems(); }
	
	private:

		/** The model this view is visualizing. */
		TSharedPtr<IObjectToPropertiesModel> PropertiesModel;

		/** Lists the selected actors */
		TSharedPtr<SExpandableArea> ActorArea;
		/** Lists the properties of the selected actor */
		TSharedPtr<SExpandableArea> PropertyArea;

		/** Tree view for replicated objects. */
		TSharedPtr<SReplicationTreeView<TSharedPtr<FReplicatedObjectData>>> ReplicatedObjects;
		/** Tree view for replicated properties. Content depends on the current object selected. */
		TSharedPtr<SReplicationTreeView<TSharedPtr<FReplicatedPropertyData>>> ReplicatedProperties;

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
		
		/**
		 * These instances can be subclasses of FReplicatedObjectData, e.g. FReplicatedObjectData_Editor.
		 * Their type can be overridden by subclasses.
		 * They only have the FReplicatedObjectData type so they can be passed efficiently to SPropertyReplicationSelectionViewer.
		 * @see GetObjectData
		 */
		TSet<TSharedPtr<FReplicatedObjectData>> ObjectRowData;
		/** The instances of ObjectRowData which do not have any parents. This acts as the item source for the tree view. */
		TArray<TSharedPtr<FReplicatedObjectData>> RootObjectRowData;
		/** Inverse map of ObjectRowData using FReplicatedObjectData::GetObjectPath as key. Contains all elements of ObjectRowData. */
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>> PathToObjectDataCache;
		
		/**
		 * These instances can be subclasses of FReplicatedPropertyData, e.g. FReplicatedPropertyData_Editor.
		 * Their type can be overridden by subclasses.
		 * They only have the FReplicatedPropertyData type so they can be passed efficiently to SPropertyReplicationSelectionViewer.
		 * @see GetPropertyData
		 */
		TArray<TSharedPtr<FReplicatedPropertyData>> PropertyRowData;
		/** The instances of ObjectRowData which do not have any parents. This acts as the item source for the tree view. */
		TArray<TSharedPtr<FReplicatedPropertyData>> RootPropertyRowData;
		/** Inverse map of PropertyRowData using FReplicatedPropertyData::GetProperty as key. Contains all elements of PropertyRowData. */
		TMap<FConcertPropertyChain, TSharedPtr<FReplicatedPropertyData>> ChainToPropertyDataCache;

		bool bIsActorAreaExpanded = false;
		bool bIsPropertyAreaExpanded = false;

		/** Used for determining the order in which properties are displayed. Default: Sort by name. */
		FSortPropertyPredicate SortPropertyRowPredicate;

		virtual TSharedRef<FReplicatedObjectData> AllocateObjectData(FSoftObjectPath ObjectPath);
		virtual TSharedRef<FReplicatedPropertyData> AllocatePropertyData(FSoftClassPath OwningClass, FConcertPropertyChain PropertyChain);

		TSharedRef<SWidget> CreateActorsSection(const FArguments& InArgs);
		TSharedRef<SWidget> CreatePropertiesSection(const FArguments& InArgs);

		/** Sets RootObjectRowData to all non-root nodes from ObjectRowData. */
		void BuildRootObjectRowData();
		void BuildRootPropertyRowData();
		
		void GetObjectRowChildren(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild);
		void GetActorComponentsAsChildRows(const AActor* ResolvedActor, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild);
		void GetPropertyRowChildren(TSharedPtr<FReplicatedPropertyData> ReplicatedPropertyData, TFunctionRef<void(TSharedPtr<FReplicatedPropertyData>)> ProcessChild);

		/** Handles how much space the 'Clients' area uses with respect to its expansion state. */
		SSplitter::ESizeRule GetActorAreaSizeRule() const { return bIsActorAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
		void OnActorAreaExpansionChanged(bool bExpanded) { bIsActorAreaExpanded = bExpanded; }

		/** Handles how much space the 'Clients' area uses with respect to its expansion state. */
		SSplitter::ESizeRule GetPropertyAreaSizeRule() const { return bIsPropertyAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
		void OnPropertyAreaExpansionChanged(bool bExpanded) { bIsPropertyAreaExpanded = bExpanded; }

		// Utils
		void SortPropertyRowArray(TArray<TSharedPtr<FReplicatedPropertyData>>& ToSort) const;
		void SetPropertyContent(EReplicatedPropertyContent Content) const;
	};

	template <typename TObjectRangeType>
	void SPropertyReplicationSelectionViewer::SetSelectedObjects(const TObjectRangeType& ObjectsToSelect, bool bIsSelected)
	{
		TArray<TSharedPtr<FReplicatedObjectData>> ObjectDataToSelect;
		Algo::Transform(ObjectsToSelect, ObjectDataToSelect, [this](const FSoftObjectPath& Path){ return PathToObjectDataCache[Path]; });
		ReplicatedObjects->SetSelectedItems(ObjectDataToSelect, bIsSelected);
	}

	template <typename TObjectRangeType>
	void SPropertyReplicationSelectionViewer::SetObjectsExpanded(const TObjectRangeType& ObjectsToSelect, bool bIsExpanded)
	{
		TArray<TSharedPtr<FReplicatedObjectData>> ObjectDataToSelect;
		Algo::Transform(ObjectsToSelect, ObjectDataToSelect, [this](const FSoftObjectPath& Path){ return PathToObjectDataCache[Path]; });
		ReplicatedObjects->SetExpandedItems(ObjectDataToSelect, bIsExpanded);
	}

	template <typename TObjectType>
	void SPropertyReplicationSelectionViewer::GetObjectChildren(const TObjectType& ReplicatedObjectData, TFunctionRef<void(const FSoftObjectPath&)> ProcessChild)
	{
		if (const TSharedPtr<FReplicatedObjectData>* Data = PathToObjectDataCache.Find(ReplicatedObjectData))
		{
			GetObjectRowChildren(*Data, [&ProcessChild](const TSharedPtr<FReplicatedObjectData>& Child){ ProcessChild(Child->GetObjectPath()); });
		}
	}
}
