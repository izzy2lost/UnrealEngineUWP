// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModuleWorkspaceAssetUserData.h"

#include "UncookedOnlyUtils.h"
#include "Module/AnimNextModule.h"
#include "Module/AnimNextModule_EditorData.h"
#include "UObject/AssetRegistryTagsContext.h"

void UAnimNextModuleWorkspaceAssetUserData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FWorkspaceOutlinerItemExports Exports;
	{
		const UAnimNextModule* ModuleOuter = CastChecked<UAnimNextModule>(GetOuter());
		const UAnimNextModule_EditorData* GraphEditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData(ModuleOuter);	
		{
			FWorkspaceOutlinerItemExport& RootAssetExport = Exports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(ModuleOuter->GetFName(), ModuleOuter));
			RootAssetExport.GetData().InitializeAsScriptStruct(FAnimNextGraphAssetOutlinerData::StaticStruct());
		}
	
		UE::AnimNext::UncookedOnly::FUtils::GetAssetOutlinerItems(GraphEditorData, Exports);
	}
	
	FString TagValue;
	FWorkspaceOutlinerItemExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
	Context.AddTag(FAssetRegistryTag(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}
