// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerHierarchy.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "TypedElementOutliner/TypedElementOutlinerItem.h"

FTypedElementOutlinerHierarchy::FTypedElementOutlinerHierarchy(FTypedElementOutlinerMode* InMode, TArray<TypedElementDataStorage::QueryHandle> InRowHandleQueries)
: ISceneOutlinerHierarchy(InMode)
, TEDSOutlinerMode(InMode)
, RowHandleQueries(InRowHandleQueries)
{

	ITypedElementDataStorageInterface* Storage = InMode->GetStorage();
	using namespace TypedElementQueryBuilder;

	// TEDS-Outliner TODO: Add Observers to track addition/removal copying the selection data from InRowHandleQueries
}

void FTypedElementOutlinerHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	ITypedElementDataStorageInterface* DataStorage = TEDSOutlinerMode->GetStorage();

	TypedElementDataStorage::DirectQueryCallback RowCollector = CreateDirectQueryCallbackBinding(
		[this, &OutItems](DSI::IDirectQueryContext& Context)
		{
			TConstArrayView<TypedElementRowHandle> Rows = Context.GetRowHandles();
			CreateItems_Internal(Rows, OutItems);
		});

	for(const TypedElementDataStorage::QueryHandle& Query : RowHandleQueries)
	{
		DataStorage->RunQuery(Query, RowCollector);
	}
}

void FTypedElementOutlinerHierarchy::CreateItems_Internal(TConstArrayView<TypedElementRowHandle>& Rows, TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	for(const TypedElementRowHandle& Row : Rows)
	{
		if (FSceneOutlinerTreeItemPtr TreeItem = Mode->CreateItemFor<FTypedElementOutlinerTreeItem>(FTypedElementOutlinerTreeItem(Row, *TEDSOutlinerMode), false))
		{
			OutItems.Add(TreeItem);
		}
	}
}

void FTypedElementOutlinerHierarchy::CreateChildren(const FSceneOutlinerTreeItemPtr& Item,
	TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const
{
	// TEDS-Outliner TODO: Implement once TEDS has hierarchy data
}

FSceneOutlinerTreeItemPtr FTypedElementOutlinerHierarchy::FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item,
	const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate)
{
	// TEDS-Outliner TODO: Implement once TEDS has hierarchy data
	return nullptr;
}

void FTypedElementOutlinerHierarchy::OnItemAdded(TypedElementRowHandle ItemRowHandle)
{
	// TEDS-Outliner TODO: Add Observers to track addition/removal getting data from the Select() query (InRowHandleQueries)
}

void FTypedElementOutlinerHierarchy::OnItemRemoved(TypedElementRowHandle ItemRowHandle)
{
	// TEDS-Outliner TODO: Add Observers to track addition/removal getting data from the Select() query (InRowHandleQueries)
}

