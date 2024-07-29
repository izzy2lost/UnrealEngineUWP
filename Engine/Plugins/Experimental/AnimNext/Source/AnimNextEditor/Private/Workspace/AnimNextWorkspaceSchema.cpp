// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceSchema.h"
#include "Module/AnimNextModule.h"
#include "Scheduler/AnimNextSchedule.h"

#define LOCTEXT_NAMESPACE "AnimNextWorkspaceSchema"

TArray<FTopLevelAssetPath> UAnimNextWorkspaceSchema::SupportedAssetClasses =
{
	UAnimNextSchedule::StaticClass()->GetClassPathName(),
	UAnimNextModule::StaticClass()->GetClassPathName()
};

FText UAnimNextWorkspaceSchema::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "AnimNext Workspace");
}

TConstArrayView<FTopLevelAssetPath> UAnimNextWorkspaceSchema::GetSupportedAssetClassPaths() const
{
	return SupportedAssetClasses;
}

#undef LOCTEXT_NAMESPACE