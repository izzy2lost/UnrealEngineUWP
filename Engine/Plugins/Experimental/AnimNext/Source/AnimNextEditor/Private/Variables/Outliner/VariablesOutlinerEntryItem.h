// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneOutlinerTreeItem.h"

class UAnimNextRigVMAssetEntry;

namespace UE::AnimNext::Editor
{

class IWorkspaceOutlinerItemDetails;

struct FVariablesOutlinerEntryItem : ISceneOutlinerTreeItem
{
	static const FSceneOutlinerTreeItemType Type;

	FVariablesOutlinerEntryItem(UAnimNextRigVMAssetEntry* InEntry);

	// Begin ISceneOutlinerTreeItem overrides
	virtual bool IsValid() const override;
	virtual FSceneOutlinerTreeItemID GetID() const override;
	virtual FString GetDisplayString() const override;
	virtual bool CanInteract() const override { return true; }
	virtual TSharedRef<SWidget> GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow) override;
	virtual FString GetPackageName() const override;
	// End ISceneOutlinerTreeItem overrides

	// Renames the item to the specified name
	void Rename(const FText& InNewName);

	// Validates the new item name
	bool ValidateName(const FText& InNewName, FText& OutErrorMessage) const;

	// Ptr to the underlying entry
	TWeakObjectPtr<UAnimNextRigVMAssetEntry> WeakEntry;
};

}
