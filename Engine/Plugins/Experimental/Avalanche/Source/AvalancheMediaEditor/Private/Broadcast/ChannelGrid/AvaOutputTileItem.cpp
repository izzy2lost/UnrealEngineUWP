// Copyright Epic Games, Inc. All Rights Reserved.

#include "Broadcast/ChannelGrid/AvaOutputTileItem.h"
#include "AvaMediaEditorStyle.h"
#include "AvalancheBroadcast.h"
#include "ClassIconFinder.h"
#include "DragDropOps/AvaOutputTileItemDragDropOp.h"
#include "MediaOutput.h"
#include "OutputDevices/AvaMediaOutputUtils.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "AvaOutputTileItem"

FAvaOutputTileItem::FAvaOutputTileItem(FName InChannelName, UMediaOutput* InMediaOutput)
	: ChannelName(InChannelName)
	, MediaOutput(InMediaOutput)
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FAvaOutputTileItem::OnMediaOutputPropertyChanged);
	FAvaOutputChannel::GetOnChannelChanged().AddRaw(this, &FAvaOutputTileItem::OnChannelChanged);
	FAvaOutputChannel::GetOnMediaOutputStateChanged().AddRaw(this, &FAvaOutputTileItem::OnMediaOutputStateChanged);
	BroadcastChangedHandle = UAvalancheBroadcast::Get().AddChangeListener(
		FOnAvaBroadcastChanged::FDelegate::CreateRaw(this, &FAvaOutputTileItem::OnBroadcastChanged));

	UpdateInfo();
}

FAvaOutputTileItem::~FAvaOutputTileItem()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	FAvaOutputChannel::GetOnChannelChanged().RemoveAll(this);
	FAvaOutputChannel::GetOnMediaOutputStateChanged().RemoveAll(this);
	UAvalancheBroadcast::Get().RemoveChangeListener(BroadcastChangedHandle);
}

const FAvaOutputChannel& FAvaOutputTileItem::GetChannel() const
{
	return UAvalancheBroadcast::Get().GetCurrentProfile().GetChannel(ChannelName);
}

FAvaOutputChannel& FAvaOutputTileItem::GetChannel()
{
	return UAvalancheBroadcast::Get().GetCurrentProfile().GetChannelMutable(ChannelName);
}

TSharedRef<SWidget> FAvaOutputTileItem::GenerateTile() const
{
	check(MediaOutput.IsValid());
	
	return SNew(SHorizontalBox)
		.ToolTipText(this, &FAvaOutputTileItem::GetToolTipText)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SImage)
			.Image(GetMediaOutputIcon())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.f, 0.f, 0.f, 0.f)
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(0.75f)
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			.VAlign(EVerticalAlignment::VAlign_Fill)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::UserSpecified)
				.UserSpecifiedScale(1.25f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FAvaOutputTileItem::GetDisplayText)
					.Justification(ETextJustify::Center)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(0.25f)
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			.VAlign(EVerticalAlignment::VAlign_Fill)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFitY)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(EVerticalAlignment::VAlign_Center)
					[
						SNew(SImage)
						.Image(this, &FAvaOutputTileItem::GetMediaOutputStatusBrush)
					]
					+SHorizontalBox::Slot()
					.Padding(2.f, 0.f, 0.f, 0.f)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(this, &FAvaOutputTileItem::GetMediaOutputStatusText)
					]
				]
			]
		];
}

FText FAvaOutputTileItem::GetDisplayText() const
{
	return MediaOutputDisplayText;
}

FText FAvaOutputTileItem::GetMediaOutputStatusText() const
{
	return MediaOutputStatusText;
}

FText FAvaOutputTileItem::GetToolTipText() const
{
	return MediaOutputToolTipText;
}

const FSlateBrush* FAvaOutputTileItem::GetMediaOutputIcon() const
{
	check(MediaOutput.IsValid());
	return FClassIconFinder::FindThumbnailForClass(MediaOutput->GetClass());
}

const FSlateBrush* FAvaOutputTileItem::GetMediaOutputStatusBrush() const
{
	return MediaOutputStatusBrush;
}

FReply FAvaOutputTileItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!UAvalancheBroadcast::Get().IsBroadcastingAnyChannel())
	{
		const bool bShouldDuplicate = MouseEvent.IsAltDown();
		
		const TSharedRef<FAvaOutputTileItemDragDropOp> DragDropOp = FAvaOutputTileItemDragDropOp::New(SharedThis(this)
				, bShouldDuplicate);
			
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}
	return FReply::Unhandled();
}

void FAvaOutputTileItem::OnMediaOutputPropertyChanged(UObject* InObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (MediaOutput.IsValid() && InObject == MediaOutput)
	{
		UpdateInfo();
	}
}

void FAvaOutputTileItem::OnChannelChanged(const FAvaOutputChannel& InChannel, EAvaChannelChange InChange)
{
	if (EnumHasAnyFlags(InChange, EAvaChannelChange::State)
		&& InChannel.IsValidChannel()
		&& InChannel.GetChannelName() == ChannelName)
	{
		UpdateInfo();
	}
}

void FAvaOutputTileItem::OnMediaOutputStateChanged(const FAvaOutputChannel& InChannel, const UMediaOutput* InMediaOutput)
{
	if (InChannel.IsValidChannel() && InChannel.GetChannelName() == ChannelName)
	{
		UpdateInfo();
	}
}

void FAvaOutputTileItem::OnBroadcastChanged(EAvaBroadcastChange InChange)
{
	// When output devices are updated, some of their status can change from offline/idle.
	if (EnumHasAnyFlags(InChange, EAvaBroadcastChange::OutputDevices))
	{
		UpdateInfo();
	}
}


void FAvaOutputTileItem::UpdateInfo()
{
	MediaOutputDisplayText = FindLatestDisplayText();
	
	const EAvaMediaOutputState OutputState = GetChannel().GetMediaOutputState(MediaOutput.Get());
	const EAvaMediaIssueSeverity Severity = GetChannel().GetMediaOutputIssueSeverity(OutputState, MediaOutput.Get());
	switch(OutputState)
	{
	case EAvaMediaOutputState::Offline:
		MediaOutputStatusText = LOCTEXT("MediaOutput_Offline", "Offline");
		MediaOutputStatusBrush = FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaOutputOffline");
		break;
	case EAvaMediaOutputState::Idle:
		MediaOutputStatusText = LOCTEXT("MediaOutput_Idle", "Idle");
		MediaOutputStatusBrush = FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaOutputIdle");
		break;
	case EAvaMediaOutputState::Preparing:
		MediaOutputStatusText = LOCTEXT("MediaOutput_Preparing", "Preparing");
		MediaOutputStatusBrush = FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaOutputPreparing");
		break;
	case EAvaMediaOutputState::Live:
		MediaOutputStatusText = LOCTEXT("MediaOutput_Live", "Live");
		if (Severity == EAvaMediaIssueSeverity::Errors || Severity == EAvaMediaIssueSeverity::Warnings)
		{
			// If severity is warning or error, add a secondary icon (yellow or red exclamation mark)
			// so the user knows to lookup the error in the tooltip.
			MediaOutputStatusBrush = FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaOutputLiveWarn");
		}
		else
		{
			MediaOutputStatusBrush = FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaOutputLive");
		}
		break;
	case EAvaMediaOutputState::Error:
	default:
		MediaOutputStatusText = LOCTEXT("MediaOutput_Errors", "Error(s)");
		MediaOutputStatusBrush = FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.MediaOutputError");
		break;
	}

	FString AllMessages;
	switch(Severity)
	{
	case EAvaMediaIssueSeverity::Errors:
		AllMessages.Append("Errors: \n");
		break;
	case EAvaMediaIssueSeverity::Warnings:
		AllMessages.Append(TEXT("Warning: \n"));
		break;
	default:
		AllMessages.Append(TEXT("Healthy\n"));
		break;
	}
	
	const TArray<FString>& Messages = GetChannel().GetMediaOutputIssueMessages(MediaOutput.Get());
	for (const FString& Message : Messages)
	{
		AllMessages.Append(Message);
		AllMessages.Append(TEXT("\n"));
	}
	MediaOutputToolTipText = FText::FromString(AllMessages);
}

FText FAvaOutputTileItem::FindLatestDisplayText() const
{
	check(MediaOutput.IsValid());
	const FString DeviceName = UE::AvaMediaOutputUtils::GetDeviceName(MediaOutput.Get());
	return !DeviceName.IsEmpty() ? FText::FromString(DeviceName) : FText::FromName(MediaOutput->GetFName());
}

#undef LOCTEXT_NAMESPACE
