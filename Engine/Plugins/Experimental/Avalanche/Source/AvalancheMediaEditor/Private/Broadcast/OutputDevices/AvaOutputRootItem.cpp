// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutputRootItem.h"
#include "AvaOutputServerItem.h"
#include "IAvaMediaModule.h"
#include "Input/Reply.h"
#include "MediaOutput.h"
#include "OutputDevices/IAvaDeviceProviderProxyManager.h"
#include "UObject/UObjectIterator.h"

void FAvaOutputRootItem::RefreshChildren()
{
	const IAvaDeviceProviderProxyManager& DeviceProviderProxyManager = IAvaMediaModule::Get().GetDeviceProviderProxyManager();

	const TSet<FString> CurrentServers = DeviceProviderProxyManager.GetServerNames();

	TSet<FString> SeenServerNames;
	SeenServerNames.Reserve(CurrentServers.Num());

	//Add local server item
	const TSharedPtr<FAvaOutputServerItem> OutputLocalServerItem = MakeShared<FAvaOutputServerItem>(DeviceProviderProxyManager.GetLocalServerName(), nullptr);
	Children.Add(OutputLocalServerItem);

	//Remove Existing Children that are Invalid
	for (TArray<FAvaOutputTreeItemPtr>::TIterator ItemIt = Children.CreateIterator(); ItemIt; ++ItemIt)
	{
		FAvaOutputTreeItemPtr Item(*ItemIt);

		//Always going to display local devices group
		if (Item == OutputLocalServerItem)
		{
			continue;
		}

		//Remove Invalid Pointers or Items that are not Output Server Items since Root can only have the Server Items as Top Level
		if (!Item.IsValid() || !Item->IsA<FAvaOutputServerItem>())
		{
			ItemIt.RemoveCurrent();
			continue;
		}

		const TSharedPtr<FAvaOutputServerItem> OutputServerItem = StaticCastSharedPtr<FAvaOutputServerItem>(Item);
		const FString UnderlyingServerName = OutputServerItem->GetServerName();

		if (!UnderlyingServerName.IsEmpty() && CurrentServers.Contains(UnderlyingServerName))
		{
			SeenServerNames.Add(UnderlyingServerName);
		}
		else
		{
			//Remove if there's no valid Underlying OutputClass or it's no longer in the set
			ItemIt.RemoveCurrent();
		}
	}
	
	//Append the New Servers that are not already in the Original Children List
	{
		TSet<FString> NewServers = CurrentServers.Difference(SeenServerNames);
		Children.Reserve(Children.Num() + NewServers.Num());
		
		for (const FString& ServerName : NewServers)
		{
			if (const TSharedPtr<const FAvaDeviceProviderDataList> ServerDeviceProviderDataList = DeviceProviderProxyManager.GetDeviceProviderDataListForServer(ServerName))
			{
				TSharedPtr<FAvaOutputServerItem> OutputServerItem = MakeShared<FAvaOutputServerItem>(ServerName, ServerDeviceProviderDataList);
				Children.Add(OutputServerItem);
			}
		}
	}
}

FText FAvaOutputRootItem::GetDisplayName() const
{
	//Root shouldn't need to give a Display Name!
	check(0);
	return FText::GetEmpty();
}

const FSlateBrush* FAvaOutputRootItem::GetIconBrush() const
{
	check(0);
	return nullptr;
}

TSharedPtr<SWidget> FAvaOutputRootItem::GenerateRowWidget()
{
	//Root shouldn't need to Generate a Row Widget
	check(0);
	return nullptr;
}

FReply FAvaOutputRootItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	check(0);
	return FReply::Unhandled();
}

UMediaOutput* FAvaOutputRootItem::AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo)
{
	check(0);
	return nullptr;
}
