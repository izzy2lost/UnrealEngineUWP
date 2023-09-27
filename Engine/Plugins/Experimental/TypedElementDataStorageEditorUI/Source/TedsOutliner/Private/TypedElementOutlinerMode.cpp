// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementOutlinerMode.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "TypedElementOutlinerHierarchy.h"

FTypedElementOutlinerMode::FTypedElementOutlinerMode(const FTypedElementOutlinerModeParams& InParams)
	: ISceneOutlinerMode(InParams.SceneOutliner)
	, RowHandleQueries(InParams.RowHandleQueries)
{
}

void FTypedElementOutlinerMode::Rebuild()
{
	Hierarchy = CreateHierarchy();
}

TUniquePtr<ISceneOutlinerHierarchy> FTypedElementOutlinerMode::CreateHierarchy()
{
	return TUniquePtr<FTypedElementOutlinerHierarchy>(new FTypedElementOutlinerHierarchy(this, RowHandleQueries));
}
