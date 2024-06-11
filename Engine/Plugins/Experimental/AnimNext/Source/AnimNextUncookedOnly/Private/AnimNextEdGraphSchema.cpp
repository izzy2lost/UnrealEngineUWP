// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextEdGraphSchema.h"

#include "AnimNextRigVMAsset.h"
#include "AnimNextRigVMAssetEntry.h"
#include "Param/AnimNextEditorParam.h"
#include "Param/AnimNextParam.h"

#define LOCTEXT_NAMESPACE "AnimNextEdGraphSchema"

void UAnimNextEdGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	Super::GetGraphDisplayInformation(Graph, DisplayInfo);
	
	if(UAnimNextRigVMAssetEntry* AssetEntry = Cast<UAnimNextRigVMAssetEntry>(Graph.GetOuter()))
	{
		DisplayInfo.DisplayName = FText::Format(LOCTEXT("GraphTabTitleFormat", "{0}: {1}"), FText::FromName(AssetEntry->GetEntryName()), FText::FromName(AssetEntry->GetTypedOuter<UAnimNextRigVMAsset>()->GetFName()));
		DisplayInfo.Tooltip = FText::Format(LOCTEXT("GraphTabTooltipFormat", "{0} in:\n{1}"), FText::FromName(AssetEntry->GetEntryName()), FText::FromString(AssetEntry->GetTypedOuter<UAnimNextRigVMAsset>()->GetPathName()));
	}
}

bool UAnimNextEdGraphSchema::IsStructEditable(UStruct* InStruct) const
{
	if (InStruct == FAnimNextEditorParam::StaticStruct() || InStruct == FAnimNextParam::StaticStruct())
	{
		return true;
	}
	return Super::IsStructEditable(InStruct);
}

#undef LOCTEXT_NAMESPACE