// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailTreeNode.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class ITableRow;
class SAvaRCControllerPanel;
class STableViewBase;
class URCController;

class FAvaRCControllerItem : public TSharedFromThis<FAvaRCControllerItem>
{
public:
	FAvaRCControllerItem(int32 InInstanceIndex, FName InAssetName, URCController* InController, const TSharedRef<IDetailTreeNode>& InTreeNode);
	
	TSharedRef<ITableRow> CreateWidget(TSharedRef<SAvaRCControllerPanel> InControllerPanel, const TSharedRef<STableViewBase>& InOwnerTable) const;

	FText GetDisplayName() const { return DisplayNameText; }
	const FNodeWidgets& GetNodeWidgets() const { return NodeWidgets; }
	
	int32 GetInstanceIndex() const { return InstanceIndex; }
	int32 GetDisplayIndex() const { return DisplayIndex; }
	FName GetAssetName() const { return AssetName; }
	
private:
	int32 InstanceIndex = 0;
	int32 DisplayIndex = 0;
	FName AssetName;
	FText DisplayNameText;
	FNodeWidgets NodeWidgets;
};
