// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRCControllerItem.h"
#include "Controller/RCController.h"
#include "IRemoteControlUIModule.h"
#include "RCVirtualProperty.h"
#include "SAvaRCControllerItemRow.h"

FAvaRCControllerItem::FAvaRCControllerItem(int32 InInstanceIndex, FName InAssetName, URCController* InController, const TSharedRef<IDetailTreeNode>& InTreeNode)
{
	AssetName = InAssetName;
	InstanceIndex = InInstanceIndex;
	
	if (InController)
	{
		const FName DisplayName = InController->DisplayName.IsNone()
			? InController->PropertyName
			: InController->DisplayName;
		
		DisplayNameText = FText::FromName(DisplayName);
		NodeWidgets = InTreeNode->CreateNodeWidgets();

		// Check if the Controller has a custom widget. In that case, overwrite the value widget with it
		if (const TSharedPtr<SWidget>& CustomControllerWidget = IRemoteControlUIModule::Get().CreateCustomControllerWidget(InController, InTreeNode->CreatePropertyHandle()))
		{
			NodeWidgets.ValueWidget = CustomControllerWidget;
		}
	}
}

TSharedRef<ITableRow> FAvaRCControllerItem::CreateWidget(TSharedRef<SAvaRCControllerPanel> InControllerPanel, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SAvaRCControllerItemRow, InControllerPanel, InOwnerTable, SharedThis(this));
}
