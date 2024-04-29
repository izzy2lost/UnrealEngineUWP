// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextRigVMWorkspaceAssetUserData.h"

#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraph_EditorData.h"
#include "UObject/AssetRegistryTagsContext.h"

void UAnimNextGraphWorkspaceAssetUserData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FWorkspaceOutlinerItemExports Exports;
	{
		const UAnimNextGraph* GraphOuter = CastChecked<UAnimNextGraph>(GetOuter());
		const UAnimNextGraph_EditorData* GraphEditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData(GraphOuter);	
		{
			FWorkspaceOutlinerItemExport& RootAssetExport = Exports.Exports.AddDefaulted_GetRef();	
			RootAssetExport.Identifier = GraphOuter->GetFName();
			RootAssetExport.ParentIdentifier = NAME_None;
			RootAssetExport.AssetPath = GraphOuter;

			RootAssetExport.Data.InitializeAsScriptStruct(FAnimNextGraphAssetOutlinerData::StaticStruct());
		}
	
		UE::AnimNext::UncookedOnly::FUtils::GetAssetOutlinerItems(GraphEditorData, Exports);
	}
	
	FString TagValue;
	FWorkspaceOutlinerItemExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
	Context.AddTag(FAssetRegistryTag(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}
