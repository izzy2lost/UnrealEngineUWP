// Copyright Epic Games, Inc. All Rights Reserved.

#include "VariablesOutlinerHierarchy.h"

#include "AnimNextRigVMAsset.h"
#include "ISceneOutlinerMode.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "Variables/IAnimNextRigVMVariableInterface.h"
#include "VariablesOutlinerAssetItem.h"
#include "VariablesOutlinerMode.h"
#include "WorkspaceAssetRegistryInfo.h"
#include "VariablesOutlinerEntryItem.h"
#include "Entries/AnimNextRigVMAssetEntry.h"
#include "Variables/SVariablesView.h"

namespace UE::AnimNext::Editor
{

FVariablesOutlinerHierarchy::FVariablesOutlinerHierarchy(ISceneOutlinerMode* Mode)
	: ISceneOutlinerHierarchy(Mode)
{
}

void FVariablesOutlinerHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
{
	SVariablesOutliner* Outliner = static_cast<FVariablesOutlinerMode* const>(Mode)->GetOutliner();
	for(const TSoftObjectPtr<UAnimNextRigVMAsset>& SoftAsset : Outliner->Assets)
	{
		UAnimNextRigVMAsset* Asset = SoftAsset.Get();
		if(Asset == nullptr)
		{
			continue;
		}

		const UAnimNextRigVMAssetEditorData* EditorData = UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(Asset);
		if (EditorData == nullptr)
		{
			continue;
		}

		if (FSceneOutlinerTreeItemPtr Item = Mode->CreateItemFor<FVariablesOutlinerAssetItem>(SoftAsset))
		{
			OutItems.Add(Item);
		}

		EditorData->ForEachEntryOfType<IAnimNextRigVMVariableInterface>([this, &OutItems](IAnimNextRigVMVariableInterface* InVariable)
		{
			if (FSceneOutlinerTreeItemPtr Item = Mode->CreateItemFor<FVariablesOutlinerEntryItem>(CastChecked<UAnimNextRigVMAssetEntry>(InVariable)))
			{
				OutItems.Add(Item);
			}
			return true;
		});
	}
}

FSceneOutlinerTreeItemPtr FVariablesOutlinerHierarchy::FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item, const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate)
{
	const FVariablesOutlinerEntryItem* TreeItem = Item.CastTo<FVariablesOutlinerEntryItem>();
	if (TreeItem == nullptr)
	{
		return nullptr;
	}

	UAnimNextRigVMAssetEntry* Entry = TreeItem->WeakEntry.Get();
	if (Entry == nullptr)
	{
		return nullptr;
	}

	UAnimNextRigVMAsset* Asset = Entry->GetTypedOuter<UAnimNextRigVMAsset>();
	if (Entry == nullptr)
	{
		return nullptr;
	}

	TSoftObjectPtr<UAnimNextRigVMAsset> SoftObjectPtr(Asset);
	uint32 Hash = GetTypeHash(SoftObjectPtr);

	if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(Hash))
	{
		return *ParentItem;
	}

	return nullptr;
}

}
