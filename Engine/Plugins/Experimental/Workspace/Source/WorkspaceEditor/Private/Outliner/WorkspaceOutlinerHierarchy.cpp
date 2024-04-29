// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceOutlinerHierarchy.h"

#include "ISceneOutlinerMode.h"
#include "Workspace.h"
#include "WorkspaceAssetRegistryInfo.h"
#include "WorkspaceOutlinerTreeItem.h"

namespace UE::Workspace
{
	FWorkspaceOutlinerHierarchy::FWorkspaceOutlinerHierarchy(ISceneOutlinerMode* Mode, const TWeakObjectPtr<UWorkspace>& InWorkspace) : ISceneOutlinerHierarchy(Mode), WeakWorkspace(InWorkspace)
	{
	}

	void FWorkspaceOutlinerHierarchy::CreateItems(TArray<FSceneOutlinerTreeItemPtr>& OutItems) const
	{
		if (const UWorkspace* Workspace = WeakWorkspace.Get())
		{
			TArray<FAssetData> AssetDataEntries; 
			Workspace->GetAssetDataEntries(AssetDataEntries);

			for (const FAssetData& AssetData : AssetDataEntries)
			{
				FString TagValue;
				if(AssetData.GetTagValue(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue))
				{
					FWorkspaceOutlinerItemExports Exports;
					FWorkspaceOutlinerItemExports::StaticStruct()->ImportText(*TagValue, &Exports, nullptr, 0, nullptr, FWorkspaceOutlinerItemExports::StaticStruct()->GetName());
					for (const FWorkspaceOutlinerItemExport& Export : Exports.Exports)
					{
						if (FSceneOutlinerTreeItemPtr Item = Mode->CreateItemFor<FWorkspaceOutlinerTreeItem>(FWorkspaceOutlinerTreeItem::FItemData(Export)))
						{
							OutItems.Add(Item);
						}
					}
				}
			}
		}		
	}

	FSceneOutlinerTreeItemPtr FWorkspaceOutlinerHierarchy::FindOrCreateParentItem(const ISceneOutlinerTreeItem& Item, const TMap<FSceneOutlinerTreeItemID, FSceneOutlinerTreeItemPtr>& Items, bool bCreate)
	{
		if (const FWorkspaceOutlinerTreeItem* TreeItem = Item.CastTo<FWorkspaceOutlinerTreeItem>())
		{
			const FName ParentIdentifier = TreeItem->Export.ParentIdentifier;
			if (ParentIdentifier != NAME_None)
			{
				if (const FSceneOutlinerTreeItemPtr* ParentItem = Items.Find(HashCombine(GetTypeHash(TreeItem->Export.AssetPath),GetTypeHash(ParentIdentifier))))
				{
					return *ParentItem;
				}
				else if(bCreate)
				{
					if (const UWorkspace* Workspace = WeakWorkspace.Get())
					{
						TArray<FAssetData> AssetDataEntries; 
						Workspace->GetAssetDataEntries(AssetDataEntries);

						if(const FAssetData* AssetDataPtr = AssetDataEntries.FindByPredicate([AssetPath = TreeItem->Export.AssetPath](const FAssetData& AssetData) { return AssetData.GetSoftObjectPath() == AssetPath; }))
						{
							FString TagValue;
							if((*AssetDataPtr).GetTagValue(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue))
							{
								FWorkspaceOutlinerItemExports Exports;
								FWorkspaceOutlinerItemExports::StaticStruct()->ImportText(*TagValue, &Exports, nullptr, 0, nullptr, FWorkspaceOutlinerItemExports::StaticStruct()->GetName());
								
								if (const FWorkspaceOutlinerItemExport* ExportPtr = Exports.Exports.FindByPredicate([ParentIdentifier](const FWorkspaceOutlinerItemExport& ItemExport)
								{
									return ItemExport.Identifier == ParentIdentifier;
								}))
								{
									Mode->CreateItemFor<FWorkspaceOutlinerTreeItem>(FWorkspaceOutlinerTreeItem::FItemData(*ExportPtr), true);
								}
							}
						}
					}
				}
			}
		}
		
		return nullptr;
	}
}
