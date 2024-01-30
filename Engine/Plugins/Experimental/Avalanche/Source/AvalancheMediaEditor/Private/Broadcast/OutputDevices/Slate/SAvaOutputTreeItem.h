// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FAvaOutputTreeItem;

class SAvaOutputTreeItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaOutputTreeItem){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedPtr<FAvaOutputTreeItem>& InOutputTreeItem);
	
protected:
	TWeakPtr<FAvaOutputTreeItem> OutputTreeItemWeak;
};
