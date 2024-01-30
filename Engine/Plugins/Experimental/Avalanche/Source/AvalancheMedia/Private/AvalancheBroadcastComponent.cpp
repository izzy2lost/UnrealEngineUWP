// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheBroadcastComponent.h"

#include "AvalancheBroadcast.h"
#include "Engine/World.h"

UAvalancheBroadcastComponent::UAvalancheBroadcastComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
}

bool UAvalancheBroadcastComponent::StartBroadcasting(FString& OutErrorMessage)
{
	UAvalancheBroadcast::Get().StartBroadcast();
	
	// Build the error/warning messages from all outputs of all channels.
	const TArray<FAvaOutputChannel*>& Channels = UAvalancheBroadcast::Get().GetCurrentProfile().GetChannels();
	for (const FAvaOutputChannel* Channel : Channels)
	{
		if (Channel->GetIssueSeverity() != EAvaMediaIssueSeverity::None)
		{
			FString OutputMessages;
			const TArray<UMediaOutput*>& Outputs = Channel->GetMediaOutputs();
			for (const UMediaOutput* Output : Outputs)
			{
				const TArray<FString>& OutputIssueMessages = Channel->GetMediaOutputIssueMessages(Output);
				const EAvaMediaIssueSeverity OutputSeverity = Channel->GetMediaOutputIssueSeverity(Channel->GetMediaOutputState(Output), Output);
				const FString OutputIssue = StaticEnum<EAvaMediaIssueSeverity>()->GetDisplayValueAsText(OutputSeverity).ToString();
				for (const FString& OutputIssueMessage : OutputIssueMessages)
				{
					OutputMessages += FString::Format(TEXT(" - Output {0}: {1}\n"),{OutputIssue,OutputIssueMessage});
				}
			}

			if (!OutputMessages.IsEmpty())
			{
				OutErrorMessage += FString::Format(TEXT("{0} on Channel {1}: \n"),
					{	StaticEnum<EAvaMediaIssueSeverity>()->GetDisplayValueAsText(Channel->GetIssueSeverity()).ToString(),
						Channel->GetChannelName().ToString()});
			}
			
			OutErrorMessage += OutputMessages;
		}
	}
	
	// Return true if any of the channels is broadcasting.
	return UAvalancheBroadcast::Get().IsBroadcastingAnyChannel();
}

bool UAvalancheBroadcastComponent::StopBroadcasting()
{
	UAvalancheBroadcast::Get().StopBroadcast();
	return true;
}

void UAvalancheBroadcastComponent::InitializeComponent()
{
	Super::InitializeComponent();
	FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UAvalancheBroadcastComponent::OnWorldBeginTearDown);
}

void UAvalancheBroadcastComponent::UninitializeComponent()
{
	FWorldDelegates::OnWorldBeginTearDown.RemoveAll(this);
	Super::UninitializeComponent();
}

void UAvalancheBroadcastComponent::OnWorldBeginTearDown(UWorld* InWorld)
{
	if (InWorld == GetWorld() && bStopBroadcastOnTearDown)
	{
		UAvalancheBroadcast::Get().StopBroadcast();
	}
}

