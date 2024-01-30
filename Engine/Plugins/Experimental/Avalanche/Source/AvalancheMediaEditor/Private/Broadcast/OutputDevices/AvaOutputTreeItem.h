// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Channel/AvaMediaOutputInfo.h"

class FAvaOutputTreeItem;
class FReply;
class IAvaOutputTreeItem;
class SWidget;
class UMediaOutput;
struct FGeometry;
struct FPointerEvent;
struct FSlateBrush;

typedef TSharedPtr<IAvaOutputTreeItem> FAvaOutputTreeItemPtr;

class IAvaOutputTreeItem : public IAvaTypeCastable, public TSharedFromThis<IAvaOutputTreeItem>
{
public:
	UE_AVA_INHERITS(IAvaOutputTreeItem, IAvaTypeCastable);

	virtual FText GetDisplayName() const = 0;

	virtual const FSlateBrush* GetIconBrush() const = 0;
	
	/** Refreshes what the Children are of this Item. (not recursive!) */
	virtual void RefreshChildren() = 0;

	virtual TSharedPtr<SWidget> GenerateRowWidget() = 0;
	
	virtual const TWeakPtr<FAvaOutputTreeItem>& GetParent() const = 0;

	virtual const TArray<FAvaOutputTreeItemPtr>& GetChildren() const = 0;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) = 0;

	/** Returns true if it is valid to add this item to the given channel. */
	virtual bool IsValidToDropInChannel(FName InTargetChannelName) = 0;

	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo) = 0;
};

class FAvaOutputTreeItem : public IAvaOutputTreeItem
{
public:
	UE_AVA_INHERITS(FAvaOutputTreeItem, IAvaOutputTreeItem);

	FAvaOutputTreeItem(const TSharedPtr<FAvaOutputTreeItem>& InParent)
		: ParentWeak(InParent)
	{
	}

	//~ Begin IAvaOutputTreeItem
	virtual ~FAvaOutputTreeItem() override {};
	virtual const TWeakPtr<FAvaOutputTreeItem>& GetParent() const override;
	virtual const TArray<FAvaOutputTreeItemPtr>& GetChildren() const override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual bool IsValidToDropInChannel(FName InTargetChannelName) override { return true; }
	//~ End IAvaOutputTreeItem

	/** Calls RefreshChildren() on the tree of item. */
	static void RefreshTree(const FAvaOutputTreeItemPtr& InItem);

protected:
	TWeakPtr<FAvaOutputTreeItem> ParentWeak;
	TArray<FAvaOutputTreeItemPtr> Children;
};
