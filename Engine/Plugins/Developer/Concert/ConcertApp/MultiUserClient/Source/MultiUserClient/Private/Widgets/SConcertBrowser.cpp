// Copyright Epic Games, Inc. All Rights Reserved.

#include "SConcertBrowser.h"

#include "IConcertClient.h"
#include "IConcertSyncClient.h"
#include "MultiUserClientUtils.h"
#include "SActiveSession.h"
#include "Widgets/Disconnected/SConcertClientSessionBrowser.h"
#include "Widgets/Disconnected/SConcertNoAvailability.h"

#include "Internationalization/Regex.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "SConcertBrowser"

void SConcertBrowser::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow, TWeakPtr<IConcertSyncClient> InSyncClient)
{
	if (!MultiUserClientUtils::HasServerCompatibleCommunicationPluginEnabled())
	{
		// Output a log.
		MultiUserClientUtils::LogNoCompatibleCommunicationPluginEnabled();

		// Show a message in the browser.
		ChildSlot.AttachWidget(SNew(SConcertNoAvailability)
			.Text(MultiUserClientUtils::GetNoCompatibleCommunicationPluginEnabledText()));

		return; // Installing a plug-in implies an editor restart, don't bother initializing the rest.
	}

	WeakConcertSyncClient = InSyncClient;
	if (TSharedPtr<IConcertSyncClient> ConcertSyncClient = WeakConcertSyncClient.Pin())
	{
		SearchedText = MakeShared<FText>(); // Will keep in memory the session browser search text between join/leave UI transitions.

		IConcertClientRef ConcertClient = ConcertSyncClient->GetConcertClient();
		check(ConcertClient->IsConfigured());
		
		ConcertClient->OnSessionConnectionChanged().AddSP(this, &SConcertBrowser::HandleSessionConnectionChanged);

		// Attach the panel corresponding the current state.
		AttachChildWidget(ConcertClient->GetSessionConnectionStatus());
	}
}

void SConcertBrowser::HandleSessionConnectionChanged(IConcertClientSession& InSession, EConcertConnectionStatus ConnectionStatus)
{
	AttachChildWidget(ConnectionStatus);
}

void SConcertBrowser::AttachChildWidget(EConcertConnectionStatus ConnectionStatus)
{
	if (TSharedPtr<IConcertSyncClient> ConcertSyncClient = WeakConcertSyncClient.Pin())
	{
		if (ConnectionStatus == EConcertConnectionStatus::Connected)
		{
			ChildSlot.AttachWidget(SNew(SActiveSession, ConcertSyncClient));
		}
		else if (ConnectionStatus == EConcertConnectionStatus::Disconnected)
		{
			ChildSlot.AttachWidget(SNew(SConcertClientSessionBrowser, ConcertSyncClient->GetConcertClient(), SearchedText));
		}
	}
}

#undef LOCTEXT_NAMESPACE
