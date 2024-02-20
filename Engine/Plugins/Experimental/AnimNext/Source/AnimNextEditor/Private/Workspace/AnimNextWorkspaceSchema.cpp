// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceSchema.h"
#include "Graph/AnimNextGraph.h"
#include "Scheduler/AnimNextSchedule.h"

#define LOCTEXT_NAMESPACE "AnimNextWorkspaceSchema"

FText UAnimNextWorkspaceSchema::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "AnimNext Workspace");
}

TConstArrayView<FTopLevelAssetPath> UAnimNextWorkspaceSchema::GetSupportedAssetClassPaths() const
{
	static const FTopLevelAssetPath Assets[] =
	{
		UAnimNextSchedule::StaticClass()->GetClassPathName(),
		UAnimNextGraph::StaticClass()->GetClassPathName(),
	};
	
	return Assets;
}

#undef LOCTEXT_NAMESPACE