// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkHubEditorStatusBar.h"

#include "Editor.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkHubMessagingModule.h"
#include "ILiveLinkModule.h"
#include "ILiveLinkSource.h"
#include "LiveLinkMessageBusSource.h"
#include "LiveLinkPreset.h"
#include "Styling/SlateIconFinder.h"
#include "Styling/SlateStyle.h"
#include "TimerManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LiveLinkHubStatusBar"

void SLiveLinkHubEditorStatusBar::Construct(const FArguments& InArgs)
{
	ILiveLinkHubMessagingModule& HubMessagingModule = FModuleManager::Get().GetModuleChecked<ILiveLinkHubMessagingModule>("LiveLinkHubMessaging");
	HubMessagingModule.OnConnectionEstablished().AddSP(this, &SLiveLinkHubEditorStatusBar::OnHubConnectionEstablished);

	LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	LiveLinkClient->OnLiveLinkSourceRemoved().AddSP(this, &SLiveLinkHubEditorStatusBar::OnSourceRemoved);

	constexpr bool bLoop = true;
	if (GEditor && GEditor->IsTimerManagerValid())
	{
		GEditor->GetTimerManager()->SetTimer(TimerHandle, FTimerDelegate::CreateSP(this, &SLiveLinkHubEditorStatusBar::CheckHubConnection), CheckConnectionIntervalSeconds, bLoop);
	}

	ChildSlot
	[
		SNew(SComboButton)
    		.ContentPadding(FMargin(6.0f, 0.0f))
    		.MenuPlacement(MenuPlacement_AboveAnchor)
    		.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.HasDownArrow(false)
			.Visibility(this, &SLiveLinkHubEditorStatusBar::GetVisibility)
    		.ButtonContent()
    		[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 3, 0)
				[
					SNew(SImage)
					.ColorAndOpacity(this, &SLiveLinkHubEditorStatusBar::GetIconColor)
					.Image(FSlateIcon("LiveLinkStyle","LiveLinkClient.Common.Icon.Small").GetIcon())
				]
				+ SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2, 0, 10, 0)
                [
                	SNew(STextBlock)
                	.Text(this, &SLiveLinkHubEditorStatusBar::GetStatusText)
                	.ToolTipText(this, &SLiveLinkHubEditorStatusBar::GetToolTipText)
                ]
    		]
	];
}

SLiveLinkHubEditorStatusBar::~SLiveLinkHubEditorStatusBar()
{
	if (ILiveLinkHubMessagingModule* HubMessagingModule = FModuleManager::Get().GetModulePtr<ILiveLinkHubMessagingModule>("LiveLinkHubMessaging"))
	{
		HubMessagingModule->OnConnectionEstablished().RemoveAll(this);
	}

	IModularFeatures::Get().OnModularFeatureRegistered().RemoveAll(this);

	if (GEditor && GEditor->IsTimerManagerValid())
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandle);
	}
}

FText SLiveLinkHubEditorStatusBar::GetStatusText() const
{
	const bool bConnectionActive = ConnectionState == EHubConnectionState::Valid;
	return bConnectionActive ? LOCTEXT("LiveLinkHubConnectionActive", "LiveLinkHub Connected") : LOCTEXT("LiveLinkHubConnectionError", "LiveLinkHub Error");
}

FText SLiveLinkHubEditorStatusBar::GetToolTipText() const
{
	const bool bConnectionActive = ConnectionState == EHubConnectionState::Valid;
	return bConnectionActive ? LOCTEXT("LiveLinkHubConnectionActiveTooltip", "LiveLinkHub is connected to this instance of Unreal Editor.") : LOCTEXT("LiveLinkHubConnectionErrorTooltip", "The LiveLinkHub connection was terminated.");
}

FSlateColor SLiveLinkHubEditorStatusBar::GetIconColor() const
{
	const bool bConnectionActive = ConnectionState == EHubConnectionState::Valid;
	return bConnectionActive ? FSlateColor(FLinearColor::Green) : FSlateColor(FLinearColor::Red);
}

void SLiveLinkHubEditorStatusBar::OnSourceRemoved(FGuid SourceId)
{
	if (HubSourceId == SourceId)
	{
		HubSourceId.Invalidate();
	}
}

void SLiveLinkHubEditorStatusBar::OnHubConnectionEstablished(FGuid SourceId)
{
	bHasConnectedOnce = true;

	HubSourceId = SourceId;
	ConnectionState = EHubConnectionState::Valid;
}

void SLiveLinkHubEditorStatusBar::CheckHubConnection()
{
	if (!HubSourceId.IsValid())
	{
		ConnectionState = Invalid;
		return;
	}

	if (LiveLinkClient->IsSourceStillValid(HubSourceId))
	{
		ConnectionState = EHubConnectionState::Valid;
		return;
	}

	ConnectionState = EHubConnectionState::Timeout;
}

EVisibility SLiveLinkHubEditorStatusBar::GetVisibility() const
{
	if (ConnectionState != EHubConnectionState::Invalid || bHasConnectedOnce)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE /*LiveLinkHubStatusBar*/
