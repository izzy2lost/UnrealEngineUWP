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
	return TedsOutlinerImpl->FindOrCreateParentItem(Item, Items, bCreate);
}