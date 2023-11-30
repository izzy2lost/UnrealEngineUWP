// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerMode.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "TypedElementOutlinerHierarchy.h"

FTypedElementOutlinerMode::FTypedElementOutlinerMode(const FTypedElementOutlinerModeParams& InParams)
	: ISceneOutlinerMode(InParams.SceneOutliner)
	, RowHandleQueries(InParams.RowHandleQueries)
{
}

FTypedElementOutlinerMode::~FTypedElementOutlinerMode()
{
	// Unregister all queries
	for(TypedElementDataStorage::QueryHandle QueryHandle : RowHandleQueries)
	{
		Storage->UnregisterQuery(QueryHandle);
	}
}


void FTypedElementOutlinerMode::Rebuild()
{
	Hierarchy = CreateHierarchy();
}

TUniquePtr<ISceneOutlinerHierarchy> FTypedElementOutlinerMode::CreateHierarchy()
{
	return TUniquePtr<FTypedElementOutlinerHierarchy>(new FTypedElementOutlinerHierarchy(this, RowHandleQueries));
}
