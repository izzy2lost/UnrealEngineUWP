// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RCSignatureTreeItemBase.h"

/**
 * Item class for the Root of all items
 * This is primarily so that the Top level items can have the same set of functionalities as the rest of the items in the tree 
 */
class FRCSignatureTreeRootItem : public FRCSignatureTreeItemBase
{
public:
	explicit FRCSignatureTreeRootItem(const TSharedPtr<SRCSignatureTree>& InSignatureTree);

	TArray<TSharedPtr<FRCSignatureTreeItemBase>>& GetChildrenMutable()
	{
		return Children;
	}

protected:
	//~ Begin FRCSignatureTreeItemBase
	virtual void BuildPathSegment(FStringBuilderBase& InBuilder) const override {}
	virtual void GenerateChildren(TArray<TSharedPtr<FRCSignatureTreeItemBase>>& OutChildren) const override;
	virtual FText GetDisplayNameText() const override;
	virtual FText GetDescription() const override;
	virtual int32 RemoveFromRegistry() override;
	//~ End FRCSignatureTreeItemBase
};
