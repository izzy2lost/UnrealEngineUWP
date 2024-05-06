// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/ReplicationWidgetFactories.h"
#include "Templates/SharedPointer.h"

struct FConcertStreamObjectAutoBindingRules;
struct FConcertObjectReplicationMap;

namespace UE::ConcertSharedSlate
{
	class IPropertySelectionSourceModel;
	class IPropertyTreeView;
	class IEditableReplicationStreamModel;
	class IObjectNameModel;
	class IReplicationStreamEditor;
	class IStreamExtender;
	class IObjectHierarchyModel;
	
	struct FCreateEditorParams;
	struct FCreatePropertyTreeViewParams;
}

namespace UE::ConcertClientSharedSlate
{
	/** Builds a similar tree hierarchy as SSubobjectEditor. Reports only components as subobjects. */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IObjectHierarchyModel> CreateObjectHierarchyForComponentHierarchy();

	/** Name model that uses editor data for determining display names: actors use their labels, components ask USubobjectDataSubsystem. */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IObjectNameModel> CreateEditorObjectNameModel();
	
	/**
	 * Wraps the passed in BaseModel and makes it transactional.
	 * All calls that modify the underlying model was wrapped with scoped transactions.
	 * 
	 * @param OwnerObject The object containing the FConcertObjectReplicationMap - used for transactions.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel(
		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>& BaseModel,
		UObject& OwnerObject
		);
	/** Simpler CreateTransactionalStreamModel overload that internally creates an UObject and sets it up automatically. */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> CreateTransactionalStreamModel();

	/** Params for creating a filterable property tree view. */
	struct FFilterablePropertyTreeViewParams
	{
		/** The columns the property view should have. The label column is always included. */
		TArray<ConcertSharedSlate::FPropertyColumnEntry> AdditionalPropertyColumns
		{
			ConcertSharedSlate::ReplicationColumns::Property::LabelColumn()
		};
		
		/** Optional initial primary sort mode for object rows */
		ConcertSharedSlate::FColumnSortInfo PrimaryPropertySort { ConcertSharedSlate::ReplicationColumns::Property::LabelColumnId, EColumnSortMode::Ascending };
		/** Optional initial secondary sort mode for object rows */
		ConcertSharedSlate::FColumnSortInfo SecondaryPropertySort { ConcertSharedSlate::ReplicationColumns::Property::LabelColumnId, EColumnSortMode::Ascending };
	};
	
	/**
	 * Creates a tree view that allows filtering of properties based on their type.
	 * 
	 * There is a combo box to the left of the search bar for managing the used filters.
	 * The user can toggle used filters on and off under the search bar.
	 */
	CONCERTCLIENTSHAREDSLATE_API TSharedRef<ConcertSharedSlate::IPropertyTreeView> CreateFilterablePropertyTreeView(FFilterablePropertyTreeViewParams Params);
}
