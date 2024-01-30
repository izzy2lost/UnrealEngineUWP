// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PCGGraph.h"

#include "PCGEditor.h"
#include "PCGGraph.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_PCGGraph"

FText UAssetDefinition_PCGGraph::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Graph");
}

TSoftClassPtr<UObject> UAssetDefinition_PCGGraph::GetAssetClass() const
{
	return UPCGGraph::StaticClass();
}

EAssetCommandResult UAssetDefinition_PCGGraph::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UPCGGraph* PCGGraph : OpenArgs.LoadObjects<UPCGGraph>())
	{
		const TSharedRef<FPCGEditor> PCGEditor = MakeShared<FPCGEditor>();
		PCGEditor->Initialize(EToolkitMode::Standalone, OpenArgs.ToolkitHost, PCGGraph);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE