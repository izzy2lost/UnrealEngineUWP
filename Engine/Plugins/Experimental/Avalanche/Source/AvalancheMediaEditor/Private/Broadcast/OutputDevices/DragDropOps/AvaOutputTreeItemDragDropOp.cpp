// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutputTreeItemDragDropOp.h"
#include "AvalancheBroadcast.h"
#include "Broadcast/OutputDevices/AvaOutputClassItem.h"
#include "MediaOutput.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "AvaOutputTreeItemDragDropOp"

TSharedRef<FAvaOutputTreeItemDragDropOp> FAvaOutputTreeItemDragDropOp::New(const TSharedPtr<FAvaOutputTreeItem>& InOutputTreeItem)
{
	TSharedRef<FAvaOutputTreeItemDragDropOp> DragDropOp = MakeShared<FAvaOutputTreeItemDragDropOp>();
	DragDropOp->Init(InOutputTreeItem);
	return DragDropOp;
}

bool FAvaOutputTreeItemDragDropOp::IsValidToDropInChannel(FName InTargetChannelName) const
{
	return OutputTreeItem.IsValid() && OutputTreeItem->IsValidToDropInChannel(InTargetChannelName);
}

FReply FAvaOutputTreeItemDragDropOp::OnChannelDrop(FName InTargetChannelName)
{
	if (const TSharedPtr<FAvaOutputTreeItem> Item = GetOutputTreeItem())
	{
		UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
		
		FScopedTransaction Transaction(LOCTEXT("OnChannelDrop", "Drop Media Output"));
		Broadcast.Modify();
		
		const UMediaOutput* const MediaOutput = Item->AddMediaOutputToChannel(InTargetChannelName, FAvaMediaOutputInfo());
		if (IsValid(MediaOutput))
		{
			return FReply::Handled();
		}
		else
		{
			Transaction.Cancel();
		}
	}
	return FReply::Unhandled();
}

void FAvaOutputTreeItemDragDropOp::Init(const TSharedPtr<FAvaOutputTreeItem>& InOutputTreeItem)
{
	OutputTreeItem = InOutputTreeItem;
	CurrentHoverText = InOutputTreeItem->GetDisplayName();
	CurrentIconBrush = InOutputTreeItem->GetIconBrush();
	CurrentIconColorAndOpacity = FSlateColor::UseForeground();
	MouseCursor = EMouseCursor::GrabHandClosed;

	SetupDefaults();
	Construct();
}

#undef LOCTEXT_NAMESPACE
