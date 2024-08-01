// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "ISceneOutlinerHierarchy.h"
#include "ISceneOutlinerMode.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"

class ITypedElementDataStorageUiInterface;
class ITypedElementDataStorageCompatibilityInterface;
struct FTypedElementWidgetConstructor;
class SWidget;

// Struct storing information on how hierarchies are handled in the TEDS Outliner
struct FTedsOutlinerHierarchyData
{
	/** A delegate used to get the parent row handle for a given row */
	DECLARE_DELEGATE_RetVal_OneParam(TypedElementDataStorage::RowHandle, FGetParentRowHandle, void* /* InColumnData */);
	
	/** A delegate used to set the parent row handle for a given row */
	DECLARE_DELEGATE_TwoParams(FSetParentRowHandle, void* /* InColumnData */, TypedElementDataStorage::RowHandle /* InParentRowHandle */);

	FTedsOutlinerHierarchyData(const UScriptStruct* InHierarchyColumn, const FGetParentRowHandle& InGetParent, const FSetParentRowHandle& InSetParent)
		: HierarchyColumn(InHierarchyColumn)
		, GetParent(InGetParent)
		, SetParent(InSetParent)
	{
	
	}

	// The column that contains the parent row handle for rows
	const UScriptStruct* HierarchyColumn;

	// Function to get parent row handle
	FGetParentRowHandle GetParent;

	// Function to set the parent row handle
	FSetParentRowHandle SetParent;
	
	// Get the default hierarchy data for the TEDS Outliner that uses FTableRowParentColumn to get the parent
	static FTedsOutlinerHierarchyData GetDefaultHierarchyData()
	{
		const FGetParentRowHandle RowHandleGetter = FGetParentRowHandle::CreateLambda([](void* InColumnData)
			{
				if(const FTableRowParentColumn* ParentColumn = static_cast<FTableRowParentColumn *>(InColumnData))
				{
					return ParentColumn->Parent;
				}

				return TypedElementDataStorage::InvalidRowHandle;
			});

		const FSetParentRowHandle RowHandleSetter = FSetParentRowHandle::CreateLambda([](void* InColumnData,
			TypedElementDataStorage::RowHandle InRowHandle)
			{
				if(FTableRowParentColumn* ParentColumn = static_cast<FTableRowParentColumn *>(InColumnData))
				{
					ParentColumn->Parent = InRowHandle;
				}

			});
		
		return FTedsOutlinerHierarchyData(FTableRowParentColumn::StaticStruct(), RowHandleGetter, RowHandleSetter);
	}
};

struct FTedsOutlinerParams
{
	FTedsOutlinerParams(SSceneOutliner* InSceneOutliner)
	: SceneOutliner(InSceneOutliner)
	, QueryDescription()
	, bUseDefaultTedsFilters(false)
	, HierarchyData(FTedsOutlinerHierarchyData::GetDefaultHierarchyData())
	, CellWidgetPurposes{TEXT("SceneOutliner.Cell"), TEXT("General.Cell")}
	{}

	SSceneOutliner* SceneOutliner;

	// The query description that will be used to populate rows in the TEDS-Outliner
	TAttribute<TypedElementDataStorage::FQueryDescription> QueryDescription;
	
	// TEDS queries that will be used to create filters in this Outliner
	// TEDS-Outliner TODO: Can we consolidate this with the SceneOutliner API to create filters? Currently has to be separate because FTEDSOutlinerFilter
	// needs a reference to the mode which is not possible since filters with the Outliner API are added before the mode is init
	TMap<FName, const TypedElementDataStorage::FQueryDescription> FilterQueries;

	// If true, this Outliner will automatically add all TEDS tags and columns as filters
	bool bUseDefaultTedsFilters;

	// If specified, this is how the TEDS Outliner will handle hierarchies. If not specified - there will be no hierarchies shown as a
	// parent-child relation in the tree view
	TOptional<FTedsOutlinerHierarchyData> HierarchyData;

	// The selection set to use for this Outliner, unset = don't propagate tree selection to the TEDS column
	TOptional<FName> SelectionSetOverride;

	// The purposes to use when generating widgets for the columns through TEDS UI
	TArray<FName> CellWidgetPurposes;
};


// This class is meant to be a model to hold functionality to create a "table viewer" in TEDS that can be
// attached to any view/UI.
// TEDS-Outliner TODO: This class still has a few outliner implementation details leaking in that should be removed
class TEDSOUTLINER_API FTedsOutlinerImpl : public TSharedFromThis<FTedsOutlinerImpl>
{

public:

	FTedsOutlinerImpl(const FTedsOutlinerParams& InParams, ISceneOutlinerMode* InMode);
	virtual ~FTedsOutlinerImpl();

	void Init();

	// TEDS construct getters
	ITypedElementDataStorageInterface* GetStorage() const;
	ITypedElementDataStorageUiInterface* GetStorageUI() const;
	ITypedElementDataStorageCompatibilityInterface* GetStorageCompatibility() const;

	TOptional<FName> GetSelectionSetName() const;

	// Delegate fired when the selection in TEDS changes, only if SelectionSetName is set
	DECLARE_MULTICAST_DELEGATE(FOnTedsOutlinerSelectionChanged)
	FOnTedsOutlinerSelectionChanged& OnSelectionChanged();

	// Delegate fired when the hierarchy changes due to item addition/removal/move
	ISceneOutlinerHierarchy::FHierarchyChangedEvent& OnHierarchyChanged();

	// Delegate to check if a certain outliner item is compatible with this TEDS Outliner Impl - set by the system using FTedsOutlinerImpl
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsItemCompatible, const ISceneOutlinerTreeItem&)
	FIsItemCompatible& IsItemCompatible();

	// Update the selection in TEDS to the input rows, only if SelectionSetName is set
	void SetSelection(const TArray<TypedElementDataStorage::RowHandle>& InSelectedRows);

	// Helper function to create a label widget for a given row
	TSharedRef<SWidget> CreateLabelWidgetForItem(TypedElementRowHandle InRowHandle) const;

	// Get the hierarchy data associated with this table viewer
	const TOptional<FTedsOutlinerHierarchyData>& GetHierarchyData();
	
	// Add an external query to the Outliner
	void AddExternalQuery(FName QueryName, const TypedElementDataStorage::FQueryDescription& InQueryDescription);
	void RemoveExternalQuery(FName QueryName);

	// Append all external queries into the given query
	void AppendExternalQueries(TypedElementDataStorage::FQueryDescription& OutQuery);

	// TEDS-Outliner TODO: This should live in TEDS long term
	// Funtion to combine 2 queries (adds to second query to the first)
	static void AppendQuery(TypedElementDataStorage::FQueryDescription& Query1, const TypedElementDataStorage::FQueryDescription& Query2);

	// Check if the given item's parent has changed (i.e ParentRowHandle does not match what the Outliner reports as the parent)
	bool HasItemParentChanged(TypedElementDataStorage::RowHandle ItemRowHandle, TypedElementDataStorage::RowHandle ParentRowHandle) const;

	// Outliner specific functionality
	void CreateItemsFromQuery(TArray<FSceneOutlinerTreeItemPtr>& OutItems, ISceneOutlinerMode* InMode) const;
	void CreateChildren(const FSceneOutlinerTreeItemPtr& Item, TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const;

	// Get the parent row for a given row
	TypedElementDataStorage::RowHandle GetParentRow(TypedElementDataStorage::RowHandle InRowHandle);

	// Recompile all queries used by this table viewer
	void RecompileQueries();

protected:
	
	void OnItemAdded(TypedElementDataStorage::RowHandle ItemRowHandle);
	void OnItemRemoved(TypedElementDataStorage::RowHandle ItemRowHandle);
	void OnItemMoved(TypedElementDataStorage::RowHandle ItemRowHandle);

	void UnregisterQueries() const;
	void ClearSelection() const;
	void Tick();

	void CreateLabelWidgetConstructors();
	void CreateFilterQueries();

	// Check if this row can be displayed in this table viewer
	bool CanDisplayRow(TypedElementDataStorage::RowHandle ItemRowHandle) const;
	
protected:
	// TEDS Storage Constructs
	ITypedElementDataStorageInterface* Storage{ nullptr };
	ITypedElementDataStorageUiInterface* StorageUi{ nullptr };
	ITypedElementDataStorageCompatibilityInterface* StorageCompatibility{ nullptr };

	FTedsOutlinerParams CreationParams;

	// Widget constructor to create the label widget
	TArray<TPair<TypedElementDataStorage::QueryHandle, TSharedPtr<FTypedElementWidgetConstructor>>> QueryToWidgetConstructorMap;

	// Widget purposes this table viewer supports
	TArray<FName> CellWidgetPurposes;
	
	// Initial query provided by user
	TAttribute<TypedElementDataStorage::FQueryDescription> InitialQueryDescription;

	// External queries that are currently active (e.g Filters)
	TMap<FName, TypedElementDataStorage::FQueryDescription> ExternalQueries;

	// Optional Hierarchy Data
	TOptional<FTedsOutlinerHierarchyData> HierarchyData;

	// Querys to track row handle collection, addition and removal
	TypedElementDataStorage::QueryHandle RowHandleQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle RowAdditionQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle RowRemovalQuery = TypedElementDataStorage::InvalidQueryHandle;

	// Query to get all child rows
	TypedElementDataStorage::QueryHandle ChildRowHandleQuery = TypedElementDataStorage::InvalidQueryHandle;

	// Query to track when a row's parent gets changed
	TypedElementDataStorage::QueryHandle UpdateParentQuery = TypedElementDataStorage::InvalidQueryHandle;
	
	// Query to get all selected rows, track selection added, track selection removed
	TypedElementDataStorage::QueryHandle SelectedRowsQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle SelectionAddedQuery = TypedElementDataStorage::InvalidQueryHandle;
	TypedElementDataStorage::QueryHandle SelectionRemovedQuery = TypedElementDataStorage::InvalidQueryHandle;
	
	TOptional<FName> SelectionSetName;
	bool bSelectionDirty = false;
	
	// Ticker for selection updates so we don't fire the delegate multiple times in one frame for multi select
	FTSTicker::FDelegateHandle TickerHandle;
	
	FOnTedsOutlinerSelectionChanged OnTedsOutlinerSelectionChanged;

	// Scene Outliner specific constructors
	ISceneOutlinerMode* SceneOutlinerMode;
	SSceneOutliner* SceneOutliner;

	// Event fired when the hierarchy changes (addition/removal/move)
	ISceneOutlinerHierarchy::FHierarchyChangedEvent HierarchyChangedEvent;

	// Delegate to check if an item is compatible with this table viewer
	FIsItemCompatible IsItemCompatibleWithTeds;
};
