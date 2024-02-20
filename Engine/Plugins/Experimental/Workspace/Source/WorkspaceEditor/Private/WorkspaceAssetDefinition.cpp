// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceAssetDefinition.h"
#include "WorkspaceEditor.h"
#include "WorkspaceSchema.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_Workspace"

FText UAssetDefinition_Workspace::GetAssetDisplayName() const
{
	return LOCTEXT("Workspace", "Workspace");
}

FText UAssetDefinition_Workspace::GetAssetDisplayName(const FAssetData& AssetData) const
{
	FString TagValue;
	if(AssetData.GetTagValue(GET_MEMBER_NAME_CHECKED(UWorkspace, SchemaClass), TagValue))
	{
		if(UClass* SchemaClass = FindObject<UClass>(nullptr, *TagValue, true))
		{
			FText DisplayName = SchemaClass->GetDefaultObject<UWorkspaceSchema>()->GetDisplayName();
			if(!DisplayName.IsEmpty())
			{
				return DisplayName;
			}
		}
	}
	
	return GetAssetDisplayName();
}

EAssetCommandResult UAssetDefinition_Workspace::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UWorkspace* Asset : OpenArgs.LoadObjects<UWorkspace>())
	{
		TSharedRef<UE::Workspace::FWorkspaceEditor> Editor = MakeShared<UE::Workspace::FWorkspaceEditor>();
		Editor->InitEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Asset);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE