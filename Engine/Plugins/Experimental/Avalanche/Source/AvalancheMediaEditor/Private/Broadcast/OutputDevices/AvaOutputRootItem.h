// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutputTreeItem.h"

class FAvaOutputRootItem : public FAvaOutputTreeItem
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaOutputRootItem, FAvaOutputTreeItem);

	FAvaOutputRootItem()
		: Super(nullptr)
	{}

	//~ Begin IAvaOutputTreeItem
	virtual void RefreshChildren() override;
private:
	virtual FText GetDisplayName() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual TSharedPtr<SWidget> GenerateRowWidget() override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo) override;
	//~ End IAvaOutputTreeItem
};
