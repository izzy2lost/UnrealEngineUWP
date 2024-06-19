// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StructUtils/InstancedStruct.h"
#include "WorkspaceAssetRegistryInfo.generated.h"

USTRUCT()
struct FWorkspaceOutlinerItemData
{
	GENERATED_BODY()

	FWorkspaceOutlinerItemData() = default;
};

USTRUCT()
struct FWorkspaceOutlinerItemExport
{
	GENERATED_BODY()

	FWorkspaceOutlinerItemExport() = default;

	UPROPERTY()
	FName Identifier;

	UPROPERTY()
	FName ParentIdentifier;

	UPROPERTY()	
	FSoftObjectPath AssetPath;

	UPROPERTY()
	TInstancedStruct<FWorkspaceOutlinerItemData> Data;

	friend uint32 GetTypeHash(const FWorkspaceOutlinerItemExport& Export)
	{
		return HashCombine(GetTypeHash(Export.AssetPath), GetTypeHash(Export.Identifier));
	}
};

namespace UE::Workspace
{

static const FLazyName ExportsWorkspaceItemsRegistryTag = TEXT("WorkspaceItemExports");

}

USTRUCT()
struct FWorkspaceOutlinerItemExports
{
	GENERATED_BODY()

	FWorkspaceOutlinerItemExports() = default;

	UPROPERTY()
	TArray<FWorkspaceOutlinerItemExport> Exports;
};
