// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaOutputServerItem.h"
#include "AvaOutputTreeItem.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FAvaOutputClassItem : public FAvaOutputTreeItem
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaOutputClassItem, FAvaOutputTreeItem);

	FAvaOutputClassItem(const TSharedPtr<FAvaOutputTreeItem>& InParent, UClass* InOutputClass)
		: Super(InParent)
		, OutputClass(InOutputClass)
	{
	}

	UClass* GetOutputClass() const
	{
		return OutputClass.Get();
	}

	//~ Begin IAvaOutputTreeItem
	virtual FText GetDisplayName() const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual void RefreshChildren() override;
	virtual TSharedPtr<SWidget> GenerateRowWidget() override;
	virtual UMediaOutput* AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo) override;
	//~ End IAvaOutputTreeItem

protected:
	TWeakObjectPtr<UClass> OutputClass;
};
