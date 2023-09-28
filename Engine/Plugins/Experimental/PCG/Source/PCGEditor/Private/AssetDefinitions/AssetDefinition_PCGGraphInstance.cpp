// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PCGGraphInstance.h"

#include "PCGGraph.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_PCGGraphInstance"

FText UAssetDefinition_PCGGraphInstance::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Graph Instance");
}

TSoftClassPtr<UObject> UAssetDefinition_PCGGraphInstance::GetAssetClass() const
{
	return UPCGGraphInstance::StaticClass();
}

#undef LOCTEXT_NAMESPACE
