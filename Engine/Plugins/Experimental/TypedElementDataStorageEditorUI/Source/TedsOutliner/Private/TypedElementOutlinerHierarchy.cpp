// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerHierarchy.h"

#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementQueryStorageInterfaces.h"
#include "TypedElementOutlinerItem.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"

FTypedElementOutlinerHierarchy::FTypedElementOutlinerHierarchy(FTypedElementOutlinerMode* InMode,
	const TSharedRef<FTedsOutlinerImpl>& InTedsOutlinerImpl)
	: ISceneOutlinerHierarchy(InMode)
	, TedsOutlinerImpl(InTedsOutlinerImpl)
{
	HierarchyChangedHandle = TedsOutlinerImpl->OnHierarchyChanged().AddLambda([this](FSceneOutlinerHierarchyChangedData EventData)
	{
		HierarchyChangedEvent.Broadcast(EventData);
	});

	TedsOutlinerImpl->RecompileQueries();
}

FTypedElementOutlinerHierarchy::~FTypedElementOutlinerHierarchy()
{
	TedsOutlinerImpl->OnHierarchyChanged().Remove(HierarchyChangedHandle);
}

void FTypedElementOutlinerHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	TedsOutlinerImpl->CreateItemsFromQuery(OutItems, Mode);
}

void FTypedElementOutlinerHierarchy::CreateChildren(const FSceneOutlinerTreeItemPtr& Item,
	TArray<FSceneOutlinerTreeItemPtr>& OutChildren) const
{
	TedsOutlinerImpl->CreateChildren(Item, OutChildren);
}

FSceneOutlinerTreeItemPtr FTypedElementOutlinerHierarchy::FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item,
	const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate)
{
	const FTypedElementOutlinerTreeItem* TEDSTreeItem = Item.CastTo<FTypedElementOutlinerTreeItem>();
	const ITypedElementDataStorageInterface* Storage = TedsOutlinerImpl->GetStorage();
	
	// If this item is not a TEDS item, we are not handling it
	if(!TEDSTreeItem)
	{
		return nullptr;
	}
	
	const TypedElementRowHandle ParentRowHandle = TedsOutlinerImpl->GetParentRow(TEDSTreeItem->GetRowHandle());

	if(!Storage->IsRowAvailable(ParentRowHandle))
	{
		return nullptr;
	}
	
	if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(ParentRowHandle))
	{
		return *ParentItem;
	}
	else if(bCreate)
	{
		return Mode->CreateItemFor<FTypedElementOutlinerTreeItem>(FTypedElementOutlinerTreeItem(ParentRowHandle, TedsOutlinerImpl), true);
	}
	
	return nullptr;
}