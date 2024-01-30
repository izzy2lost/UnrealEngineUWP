// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutlinerItemAction.h"
#include "Engine/EngineTypes.h"

class FAvaOutliner;

enum class EItemDropZone;

enum class EAvaOutlinerAddItemFlags : uint8
{
	None        = 0,
	/** Also add the children of the given item even if they were not made into their own Add Item Action */
	AddChildren = 1 << 0,
	/** Select this Item on Add */
	Select      = 1 << 1,
	/** Make a Transaction for this Action */
	Transact    = 1 << 2,
};

ENUM_CLASS_FLAGS(EAvaOutlinerAddItemFlags);

struct FAvaOutlinerAddItemParams
{
	FAvaOutlinerAddItemParams(const FAvaOutlinerItemPtr& InItem = nullptr
			, EAvaOutlinerAddItemFlags InFlags            = EAvaOutlinerAddItemFlags::None
			, const FAvaOutlinerItemPtr& InRelativeItem   = nullptr
			, TOptional<EItemDropZone> InRelativeDropZone = TOptional<EItemDropZone>())
		: Item(InItem)
		, RelativeItem(InRelativeItem)
		, RelativeDropZone(InRelativeDropZone)
		, Flags(InFlags)
		, SelectionFlags(EAvaOutlinerItemSelectionFlags::None)
	{
	}

	/** The Item to Add */
	FAvaOutlinerItemPtr Item;

	/** The Item to use as base in where to place the Item */
	FAvaOutlinerItemPtr RelativeItem;

	/** The Placement Order from the Relative Item (Onto/Inside, Above, Below) */
	TOptional<EItemDropZone> RelativeDropZone;

	/** Some Extra Flags for what to do when Adding or After Adding the Items */
	EAvaOutlinerAddItemFlags Flags;

	/** Flags to Indicate how we should Select the Item. This only applies if bool bSelectItem is true */
	EAvaOutlinerItemSelectionFlags SelectionFlags;

	/** Optional Transform override Rule when Attaching Items */
	TOptional<FAttachmentTransformRules> AttachmentTransformRules;
};

/**
 * Item Action responsible of adding an item to the Tree under a given optional Parent.
 * If Parent is null, it is added as a Top Level Item.
 */
class AVALANCHEOUTLINER_API FAvaOutlinerAddItem : public IAvaOutlinerAction
{
public:
	UE_AVA_INHERITS(FAvaOutlinerAddItem, IAvaOutlinerAction);

	FAvaOutlinerAddItem(const FAvaOutlinerAddItemParams& InAddItemParams);

	//~ Begin IAvaOutlinerAction
	virtual bool ShouldTransact() const override;
	virtual void Execute(FAvaOutliner& InOutliner) override;
	virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& InReplacementMap, bool bRecursive) override;
	//~ End IAvaOutlinerAction

protected:
	FAvaOutlinerAddItemParams AddParams;
};
