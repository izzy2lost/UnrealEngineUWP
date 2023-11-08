// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/IReplicationStreamViewer.h"

#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/ObjectViewer/Tree/SReplicationTreeView.h"
#include "Replication/Editor/View/ObjectViewer/Tree/SelectionViewerColumns.h"
#include "SSubobjectAndPropertySection.h"

#include "Algo/Transform.h"
#include "Misc/Optional.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SExpandableArea;
class SWidgetSwitcher;
struct FSoftClassPath;
struct FSoftObjectPath;

namespace UE::ConcertClientSharedSlate
{
	class SSubobjectAndPropertySection;
	class FReplicatedPropertyData;
	class FReplicatedObjectData;
	class IEditableObjectToPropertiesModel;
	class IReplicationSubobjectView;
	class IObjectToPropertiesModel;
	class ISubobjectModel;
	class SReplicatedPropertiesView;
	
	/**
	 * Root widget for viewing UMultiUserPropertyReplicationSelection.
	 * This widget knows how to display IObjectToPropertiesModel.
	 * 
	 * The underlying data is modified by SObjectToPropertyEditor, which uses this widget's extension
	 * points to call functions on IEditableObjectToPropertiesModel.
	 *
	 * Important: this view should be possible to be built in programs, so it should not reference things like AActor,
	 * UActorComponent, ResolveObject, etc. directly. 
	 */
	class CONCERTCLIENTSHAREDSLATE_API SReplicationStreamViewer : public IReplicationStreamViewer
	{
	public:

		SLATE_BEGIN_ARGS(SReplicationStreamViewer)
		{}
			/** Additional columns to add to the object view */
			SLATE_ARGUMENT(TArray<ReplicationColumns::FReplicationTopLevelObjectColumn>, AdditionalObjectColumns)
			/** Additional columns to add to the property view */
			SLATE_ARGUMENT(TArray<ReplicationColumns::FReplicationPropertyColumn>, AdditionalPropertyColumns)

			/** Optional. Placed between root object outliner and property editor. */
			SLATE_ARGUMENT(TSharedPtr<IReplicationSubobjectView>, SubobjectView)
			/** Optional. If set, this determines the children nested under the root objects. */
			SLATE_ARGUMENT(TSharedPtr<ISubobjectModel>, SubobjectModel)

			/** Optional. Called when the delete key is pressed in the object view. */
			SLATE_EVENT(SReplicationTreeView<FReplicatedObjectData>::FDeleteItems, OnDeleteObjects)
		
			/** Called to generate the context menu for objects. */
			SLATE_EVENT(FOnContextMenuOpening, OnObjectsContextMenuOpening)

			/** Optional. Used for determining the order in which properties are displayed. */
			SLATE_EVENT(FSortPropertyPredicate, SortPropertyRowPredicate)
		
			/** Optional widget to add to the left of the object list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfObjectSearchBar)
			/** Optional widget to add to the left of the property list search bar. */
			SLATE_NAMED_SLOT(FArguments, LeftOfPropertySearchBar)

			/** Optional text to display when no object is in the outliner. Defaults to "No objects to display." "*/
			SLATE_ATTRIBUTE(FText, NoOutlinerObjects)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IObjectToPropertiesModel> InPropertiesModel);

		//~ Begin IReplicationStreamViewer Interface
		virtual void Refresh() override;
		virtual TArray<FSoftObjectPath> GetSelectedTopLevelObjects() const override;
		virtual TArray<FSoftObjectPath> GetObjectsBeingPropertyEdited() const override;
		//~ End IReplicationStreamViewer Interface

		void RefreshObjectData();
		void RefreshSubobjectData();
		void RefreshPropertyData();

		/** Selects the given objects from the top level view, if applicable. */
		void SelectTopLevelObjects(TConstArrayView<FSoftObjectPath> Objects);

		/** Expands the given objects, recursively if desired. */
		void ExpandObjects(TConstArrayView<FSoftObjectPath> Objects, bool bRecursive);
		
		/** Clears all objects selected in the subobject view, if there is one. */
		void ClearSubobjectSelection();

		/** @return Gets the root objects selected in the outliner; the subobject view chooses which of these objects (or their subobjects) end up in GetSelectedObjectShowingProperties. */
		TArray<TSharedPtr<FReplicatedObjectData>> GetSelectedOutlinerObjects() const { return ReplicatedObjects->GetSelectedItems(); }
		
	private:

		/** The model this view is visualizing. */
		TSharedPtr<IObjectToPropertiesModel> PropertiesModel;
		/** Can be null. If set, this determines the children nested under the root objects. */
		TSharedPtr<ISubobjectModel> SubobjectModel;

		/** Lists the properties of the selected actor */
		TSharedPtr<SExpandableArea> PropertyArea;
		/** Edits the property list and (optionally) exposes subobjects of the selected root object. */
		TSharedPtr<SSubobjectAndPropertySection> SubobjectAndPropertySection;

		/** Tree view for replicated objects. */
		TSharedPtr<SReplicationTreeView<FReplicatedObjectData>> ReplicatedObjects;
		
		/** All object row data */
		TArray<TSharedPtr<FReplicatedObjectData>> AllObjectRowData;
		/** The instances of ObjectRowData which do not have any parents. This acts as the item source for the tree view. */
		TArray<TSharedPtr<FReplicatedObjectData>> RootObjectRowData;
		/** Inverse map of ObjectRowData using FReplicatedObjectData::GetObjectPath as key. Contains all elements of ObjectRowData. */
		TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>> PathToObjectDataCache;

		bool bIsPropertyAreaExpanded = false;

		static TSharedRef<FReplicatedObjectData> AllocateObjectData(FSoftObjectPath ObjectPath);

		// Widget creation helpers
		TSharedRef<SWidget> CreateContentWidget(const FArguments& InArgs);
		TSharedRef<SWidget> CreateOutlinerSection(const FArguments& InArgs);
		TSharedRef<SWidget> CreatePropertiesSection(const FArguments& InArgs);

		/** Sets RootObjectRowData to all non-root nodes from ObjectRowData. */
		void BuildRootObjectRowData();

		/** Creates an item for every object in the hierarchy of ReplicatedObjectData */
		void BuildObjectHierarchyIfNeeded(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TMap<FSoftObjectPath, TSharedPtr<FReplicatedObjectData>>& NewPathToObjectDataCache);
		void GetObjectRowChildren(TSharedPtr<FReplicatedObjectData> ReplicatedObjectData, TFunctionRef<void(TSharedPtr<FReplicatedObjectData>)> ProcessChild);
		
		/** Handles how much space the 'Clients' area uses with respect to its expansion state. */
		SSplitter::ESizeRule GetPropertyAreaSizeRule() const { return bIsPropertyAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
		void OnPropertyAreaExpansionChanged(bool bExpanded) { bIsPropertyAreaExpanded = bExpanded; }
	};
}
