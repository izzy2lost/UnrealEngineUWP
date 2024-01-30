// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Channel/AvaOutputChannel.h"

struct FAvaOutputChannel;
class FAvaOutputTileItem;
class UMediaOutput;

using FAvaOutputTileItemPtr = TSharedPtr<FAvaOutputTileItem>;

class FAvaOutputTileItem : public TSharedFromThis<FAvaOutputTileItem>
{
public:

	FAvaOutputTileItem(FName InChannelName, UMediaOutput* InMediaOutput);
	~FAvaOutputTileItem();
	
	const FAvaOutputChannel& GetChannel() const;
	FAvaOutputChannel& GetChannel();
	
	UMediaOutput* GetMediaOutput() const { return MediaOutput.Get(); }

	TSharedRef<SWidget> GenerateTile() const;	

	FText GetDisplayText() const;
	FText GetMediaOutputStatusText() const;
	FText GetToolTipText() const;

	const FSlateBrush* GetMediaOutputIcon() const;
	const FSlateBrush* GetMediaOutputStatusBrush() const;

	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	
protected:

	void OnMediaOutputPropertyChanged(UObject* InObject, FPropertyChangedEvent& PropertyChangedEvent);
	void OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange);
	void OnMediaOutputStateChanged(const FAvaOutputChannel& InChannel, const UMediaOutput* InMediaOutput);
	void OnBroadcastChanged(EAvaBroadcastChange InChange);

	void UpdateInfo();
	
	FText FindLatestDisplayText() const;

protected:
	
	FName ChannelName = NAME_None;

	TWeakObjectPtr<UMediaOutput> MediaOutput;
	
	FText MediaOutputDisplayText;
	FText MediaOutputStatusText;
	FText MediaOutputToolTipText;
	
	const FSlateBrush* MediaOutputStatusBrush = nullptr;

	FDelegateHandle BroadcastChangedHandle;
};

