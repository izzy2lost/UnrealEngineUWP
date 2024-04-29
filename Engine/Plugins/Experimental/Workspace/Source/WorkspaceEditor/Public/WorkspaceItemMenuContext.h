// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkspaceAssetRegistryInfo.h"
#include "WorkspaceItemMenuContext.generated.h"

UCLASS()
class WORKSPACEEDITOR_API UWorkspaceItemMenuContext : public UObject
{
	GENERATED_BODY()
public:
	TArray<FWorkspaceOutlinerItemExport> SelectedExports;
};
