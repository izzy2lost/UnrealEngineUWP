// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutputServerItem.h"

#include "AvaOutputClassItem.h"
#include "AvalancheBroadcast.h"
#include "AvalancheMediaEditorSettings.h"
#include "IMediaIOCoreModule.h"
#include "MediaOutput.h"
#include "OutputDevices/AvaMediaOutputUtils.h"
#include "ScopedTransaction.h"
#include "Slate/SAvaOutputTreeItem.h"
#include "Styling/AppStyle.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "AvaOutputServerItem"

FString FAvaOutputServerItem::GetServerName() const
{
	return ServerName;
}

const FAvaDeviceProviderData* FAvaOutputServerItem::GetDeviceProviderData(FName InDeviceProviderName) const
{
	if (!DeviceProviderDataList.IsValid())
	{
		return nullptr;
	}

	for (const FAvaDeviceProviderData& DeviceProviderData : DeviceProviderDataList->DeviceProviders)
	{
		if (DeviceProviderData.Name == InDeviceProviderName)
		{
			return &DeviceProviderData;
		}
	}

	return nullptr;
}

FText FAvaOutputServerItem::GetDisplayName() const
{
	return FText::FromString(ServerName);
}

const FSlateBrush* FAvaOutputServerItem::GetIconBrush() const
{
	return FAppStyle::Get().GetBrush(TEXT("Icons.Server"));
}

void FAvaOutputServerItem::RefreshChildren()
{
	if (!IMediaIOCoreModule::IsAvailable())
	{
		Children.Reset();
		return;
	}

	// Make a set of all allowed Media Output Classes.
	// It may have all classes or be restricted to those with the MediaIOCustomLayout MetaData.
	TSet<UClass*> CurrentOutputClasses;
	for (UClass* const Class : TObjectRange<UClass>())
	{
		const bool bIsMediaOutputClass = Class->IsChildOf(UMediaOutput::StaticClass()) && Class != UMediaOutput::StaticClass();
		const bool bHasDeviceProvider = UE::AvaMediaOutputUtils::HasDeviceProviderName(Class);

		if (bIsMediaOutputClass && (UAvalancheMediaEditorSettings::Get().bBroadcastShowAllMediaOutputClasses || bHasDeviceProvider))
		{
			CurrentOutputClasses.Add(Class);
		}
	}

	TSet<UClass*> SeenOutputClasses;
	SeenOutputClasses.Reserve(CurrentOutputClasses.Num());

	//Remove Existing Children that are Invalid
	for (TArray<FAvaOutputTreeItemPtr>::TIterator ItemIt = Children.CreateIterator(); ItemIt; ++ItemIt)
	{
		FAvaOutputTreeItemPtr Item(*ItemIt);

		//Remove Invalid Pointers or Items that are not Output Class Items since Root can only have the Class Items as Top Level
		if (!Item.IsValid() || !Item->IsA<FAvaOutputClassItem>())
		{
			ItemIt.RemoveCurrent();
			continue;
		}

		const TSharedPtr<FAvaOutputClassItem> OutputClassItem = StaticCastSharedPtr<FAvaOutputClassItem>(Item);
		UClass* const UnderlyingOutputClass = OutputClassItem->GetOutputClass();

		if (UnderlyingOutputClass && CurrentOutputClasses.Contains(UnderlyingOutputClass))
		{
			SeenOutputClasses.Add(UnderlyingOutputClass);
		}
		else
		{
			//Remove if there's no valid Underlying OutputClass or it's no longer in the set
			ItemIt.RemoveCurrent();
		}
	}

	//Append the New Output Classes that are not already in the Original Children List
	{
		TSharedPtr<FAvaOutputServerItem> This = SharedThis(this);
		TArray<UClass*> NewOutputClasses = CurrentOutputClasses.Difference(SeenOutputClasses).Array();
		Children.Reserve(Children.Num() + NewOutputClasses.Num());

		for (UClass* const NewOutputClass : NewOutputClasses)
		{
			TSharedPtr<FAvaOutputClassItem> OutputClassItem = MakeShared<FAvaOutputClassItem>(This, NewOutputClass);
			Children.Add(OutputClassItem);
		}
	}
}

TSharedPtr<SWidget> FAvaOutputServerItem::GenerateRowWidget()
{
	return SNew(SAvaOutputTreeItem, SharedThis(this));
}

UMediaOutput* FAvaOutputServerItem::AddMediaOutputToChannel(FName InTargetChannel, const FAvaMediaOutputInfo& InOutputInfo)
{
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
