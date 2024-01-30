// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOutputTileItemDragDropOp.h"
#include "AvalancheBroadcast.h"
#include "MediaOutput.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AvaOutputTileItemDragDropOp"

TSharedRef<FAvaOutputTileItemDragDropOp> FAvaOutputTileItemDragDropOp::New(const FAvaOutputTileItemPtr& InOutputClassItem, bool bInIsDuplicating)
{
	TSharedRef<FAvaOutputTileItemDragDropOp> DragDropOp = MakeShared<FAvaOutputTileItemDragDropOp>();
	DragDropOp->Init(InOutputClassItem, bInIsDuplicating);
	return DragDropOp;
}

bool FAvaOutputTileItemDragDropOp::IsValidToDropInChannel(FName InTargetChannelName) const
{
	if (!OutputTileItem.IsValid())
	{
		return false;
	}

	// Don't allow dropping a remote output in a preview channel
	if (UAvalancheBroadcast::Get().GetChannelType(InTargetChannelName) == EAvaBroadcastChannelType::Preview
		&& OutputTileItem->GetChannel().IsMediaOutputRemote(OutputTileItem->GetMediaOutput()))
	{
		return false;
	}
	
	// Allow Duplicate on Same Channel
	return (bIsDuplicating || OutputTileItem->GetChannel().GetChannelName() != InTargetChannelName);
}

FReply FAvaOutputTileItemDragDropOp::OnChannelDrop(FName InTargetChannelName)
{
	if (TSharedPtr<FAvaOutputTileItem> Tile = GetOutputTileItem())
	{
		UAvalancheBroadcast& Broadcast = UAvalancheBroadcast::Get();
		
		FScopedTransaction Transaction(LOCTEXT("OnChannelDrop", "Drop Media Output"));
		Broadcast.Modify();
		
		UMediaOutput* const SourceMediaOutput = Tile->GetMediaOutput();

		if (!IsValid(SourceMediaOutput))
		{
			Transaction.Cancel();
			return FReply::Unhandled();
		}

		FAvaOutputChannel& TargetChannel = Broadcast.GetCurrentProfile().GetChannelMutable(InTargetChannelName);
		FAvaOutputChannel& SourceChannel = Tile->GetChannel();
		FAvaMediaOutputInfo MediaOutputInfo = SourceChannel.GetMediaOutputInfo(SourceMediaOutput);

		// Don't allow dropping a remote output in a preview channel
		if (Broadcast.GetChannelType(InTargetChannelName) == EAvaBroadcastChannelType::Preview && MediaOutputInfo.IsRemote())
		{
			Transaction.Cancel();
			return FReply::Unhandled();
		}
		
		if (bIsDuplicating)
		{
			UMediaOutput* const NewMediaOutput = DuplicateObject<UMediaOutput>(SourceMediaOutput
				, SourceMediaOutput->GetOuter()
				, NAME_None);
			
			check(NewMediaOutput);
			MediaOutputInfo.Guid = FGuid::NewGuid();	// Allocate a new guid for the duplicate.
			TargetChannel.AddMediaOutput(NewMediaOutput, MediaOutputInfo);
		}
		else
		{
			SourceChannel.RemoveMediaOutput(SourceMediaOutput);
			TargetChannel.AddMediaOutput(SourceMediaOutput, MediaOutputInfo);
		}

		return FReply::Handled();
	}
	return FReply::Unhandled();
}


void FAvaOutputTileItemDragDropOp::Init(const FAvaOutputTileItemPtr& InOutputTileItem, bool bInIsDuplicating)
{
	OutputTileItem = InOutputTileItem;
	bIsDuplicating = bInIsDuplicating;
	
	CurrentHoverText = InOutputTileItem->GetDisplayText();
	CurrentIconBrush = InOutputTileItem->GetMediaOutputIcon();
	
	CurrentIconColorAndOpacity = FSlateColor::UseForeground();
	MouseCursor = EMouseCursor::GrabHandClosed;

	SetupDefaults();
	Construct();
}

#undef LOCTEXT_NAMESPACE
