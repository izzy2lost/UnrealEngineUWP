// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationJoinedView.h"

#include "ConcertLogGlobal.h"
#include "IConcertSyncClient.h"
#include "SReplicationClientView.h"
#include "Replication/MultiUserReplicationManager.h"
#include "SSelectClientViewComboButton.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Replication/Client/RemoteReplicationClient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicationJoinedWidget"

namespace UE::MultiUserClient
{
	void SReplicationJoinedView::Construct(
		const FArguments& InArgs,
		TSharedRef<FMultiUserReplicationManager> InReplicationManager,
		TSharedRef<IConcertSyncClient> InClient
		)
	{
		Client = MoveTemp(InClient);
		ReplicationManager = MoveTemp(InReplicationManager);

		ChildSlot
		[
			SAssignNew(ClientViewSwitcher, SWidgetSwitcher)
			+SWidgetSwitcher::Slot()
			[
				SNew(SReplicationClientView)
				.GetReplicationClient_Lambda([this](){ return &ReplicationManager->GetClientManager()->GetLocalClient(); })
				.AdditionalToolbarWidgets()
				[
					MakeClientSelectionArea()
				]
			]
		];
		
		RefreshClientViewSwitcher();
		ReplicationManager->GetClientManager()->OnRemoteClientsChanged().AddSP(this, &SReplicationJoinedView::RefreshClientViewSwitcher);
		
		// Show notifications about changing authority
		ReplicationManager->GetAuthorityPolicy()->OnAuthorityRequestSent_AnyThread().AddSP(this, &SReplicationJoinedView::OnAuthorityRequestSent_AnyThread);
		ReplicationManager->GetAuthorityPolicy()->OnAuthorityResponseReceived_AnyThread().AddSP(this, &SReplicationJoinedView::OnAuthorityResponseReceived_AnyThread);
	}

	void SReplicationJoinedView::RefreshClientViewSwitcher()
	{
		const int32 ActiveViewIndex = ClientViewSwitcher->GetActiveWidgetIndex();
		const FGuid DisplayedClientId = ActiveViewIndex == 0
			? Client->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId()
			: [this, &ActiveViewIndex]() 
			{
				for (const TPair<FGuid, int32>& Pair : RemoteClientToWidgetSwitcherIndex)
				{
					if (Pair.Value == ActiveViewIndex)
					{
						return Pair.Key;
					}
				}
				return FGuid{};
			}();
		
		// Retain the old SWidgets so their transient state, like highlights, are retained when the remote clients change.
		TArray<TSharedRef<SWidget>> OldClientWidgets;
		OldClientWidgets.Reserve(ClientViewSwitcher->GetNumWidgets());
		while (ClientViewSwitcher->GetNumWidgets() > 0)
		{
			const TSharedRef<SWidget> Widget = ClientViewSwitcher->GetWidget(0).ToSharedRef();
			OldClientWidgets.Add(Widget);
			ClientViewSwitcher->RemoveSlot(Widget);
		}
		checkf(OldClientWidgets.Num() >= 1, TEXT("Was supposed to contain local client widget"));

		ClientViewSwitcher->AddSlot()
			[
				OldClientWidgets[0]
			];
		RebuildClientViewSwitcherChildren(OldClientWidgets);

		// Need to change view if the displayed client was removed
		const bool bDisplayedClientWasRemoved = RemoteClientToWidgetSwitcherIndex.Contains(DisplayedClientId); 
		if (bDisplayedClientWasRemoved)
		{
			// Will show the local client
			ClientViewSwitcher->SetActiveWidgetIndex(0);
		}
		
		// ~OldClientWidgets now disposes of the widgets for which no client exists anymore
	}

	void SReplicationJoinedView::RebuildClientViewSwitcherChildren(const TArray<TSharedRef<SWidget>> OldClientWidgets)
	{
		TMap<FGuid, int32> OldRemoteClientToWidgetSwitcherIndex = MoveTemp(RemoteClientToWidgetSwitcherIndex);
        for (const FRemoteReplicationClient& RemoteClient : ReplicationManager->GetClientManager()->GetRemoteClients())
        {
        	const FGuid& EndpointId = RemoteClient.GetRemoteEndpointId();
        	
        	const int32* ExistingClientWidgetIndex = OldRemoteClientToWidgetSwitcherIndex.Find(EndpointId);
        	if (ExistingClientWidgetIndex)
        	{
        		ClientViewSwitcher->AddSlot()
        		[
        			OldClientWidgets[*ExistingClientWidgetIndex]
        		];
        	}
        	else
        	{
        		ClientViewSwitcher->AddSlot()
        		[
        			SNew(SReplicationClientView)
        			.GetReplicationClient_Lambda([this, EndpointId = RemoteClient.GetRemoteEndpointId()]()
        			{
        				// It is unsafe to simply capture RemoteClient because the containing TArray may reallocate its location
        				return ReplicationManager->GetClientManager()->FindRemoteClient(EndpointId);
        			})
        			.AdditionalToolbarWidgets()
        			[
        				MakeClientSelectionArea()
        			]
        		];
        	}

        	const int32 NewIndex = ClientViewSwitcher->GetNumWidgets() - 1;
        	RemoteClientToWidgetSwitcherIndex.Add(EndpointId, NewIndex);
        }
	}

	TSharedRef<SWidget> SReplicationJoinedView::MakeClientSelectionArea()
	{
		return SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("ClientView.ToolTip", "Select the client(s) of which you want to see the registered objects & properties."))

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ClientView.Label", "Client View"))
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f)
			[
				SNew(SBox)
				.MinDesiredWidth(200.f)
				[
					MakeClientSelectionComboBox()
				]
			];
	}

	TSharedRef<SWidget> SReplicationJoinedView::MakeClientSelectionComboBox()
	{
		return SNew(SSelectClientViewComboButton)
		
			// Returning client info
			.Client(Client->GetConcertClient())
			.SelectableClients_Lambda([this]()
			{
				const TArray<FRemoteReplicationClient>& RemoteClients = ReplicationManager->GetClientManager()->GetRemoteClients();
				TArray<FGuid> Result;
				Algo::Transform(RemoteClients, Result, [](const FRemoteReplicationClient& InClient){ return InClient.GetRemoteEndpointId(); });

				const TSharedPtr<IConcertClientSession> CurrentSession = Client->GetConcertClient()->GetCurrentSession();
				Result.Sort([&CurrentSession](const FGuid& Left, const FGuid& Right)
				{
					FConcertSessionClientInfo LeftInfo;
					FConcertSessionClientInfo RightInfo;
					CurrentSession->FindSessionClient(Left, LeftInfo);
					CurrentSession->FindSessionClient(Right, RightInfo);
					return LeftInfo.ClientInfo.DisplayName <= RightInfo.ClientInfo.DisplayName;
				});
				Result.Insert(CurrentSession->GetSessionClientEndpointId(), 0);
				return Result;
			})
			.CurrentSelection_Lambda([this]()
			{
				const TOptional<FGuid> RemoteClient = GetRemoteClientBySwitcherIndex(ClientViewSwitcher->GetActiveWidgetIndex());
				return RemoteClient
					? *RemoteClient
					: Client->GetConcertClient()->GetCurrentSession()->GetSessionClientEndpointId();
			})
			.OnSelectClient_Lambda([this](const FGuid& InClientId)
			{
				const int32* RemoteClientIndex = RemoteClientToWidgetSwitcherIndex.Find(InClientId);
				// If it's not a remote client, it's the local client
				const int32 ActiveWidgetIndex = RemoteClientIndex ? *RemoteClientIndex : 0;
				ClientViewSwitcher->SetActiveWidgetIndex(ActiveWidgetIndex);
			})
		;
	}

	TOptional<FGuid> SReplicationJoinedView::GetRemoteClientBySwitcherIndex(int32 WidgetSwitcherIndex) const
	{
		for (const TPair<FGuid, int32>& Pair : RemoteClientToWidgetSwitcherIndex)
		{
			if (Pair.Value == WidgetSwitcherIndex)
			{
				return { Pair.Key };
			}
		}
		return {};
	}

	void SReplicationJoinedView::OnAuthorityRequestSent_AnyThread(
		const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request
		)
	{
		ExecuteOnGameThread(TEXT("OnAuthorityRequestSent"), [WeakThis = TWeakPtr<SReplicationJoinedView>(SharedThis(this)), Request]()
		{
			if (const TSharedPtr<SReplicationJoinedView> ThisPin = WeakThis.Pin())
			{
				FNotificationInfo NotificationInfo(LOCTEXT("Authority.InProgress.Text", "Requesting authority."));
				NotificationInfo.bUseThrobber = true;
				NotificationInfo.bUseSuccessFailIcons = true;
				NotificationInfo.bFireAndForget = false;
				ThisPin->AuthorityChangeNotification = FSlateNotificationManager::Get().AddNotification(NotificationInfo);
				ThisPin->AuthorityChangeNotification->SetSubText(
					FText::Format(LOCTEXT("Authority.InProgress.SubTextFmt", "Taking {0}|plural(one=object,other\nReleasing {1}|plural(one=object,other=objects)"),
						Request.TakeAuthority.Num(),
						Request.ReleaseAuthority.Num()
					));
			}
		});
	}

	void SReplicationJoinedView::OnAuthorityResponseReceived_AnyThread(
		const ConcertSyncClient::Replication::FAuthorityChangeRequest& Request,
		const ConcertSyncClient::Replication::FAuthorityChangeResponse& Response
		)
	{
		ExecuteOnGameThread(TEXT("OnAuthorityResponseReceived"), [WeakThis = TWeakPtr<SReplicationJoinedView>(SharedThis(this)), Request, Response]()
		{
			if (const TSharedPtr<SReplicationJoinedView> ThisPin = WeakThis.Pin()
				; ThisPin && ensure(ThisPin->AuthorityChangeNotification))
			{
				const bool bSuccess = Response.RejectedObjects.IsEmpty();
				const FText Text = bSuccess
					? LOCTEXT("Authority.Text.Accepted", "Authority change accepted.")
					: LOCTEXT("Authority.Text.Rejection", "Authority change had rejections.");
				const FText SubText = bSuccess
					? FText::GetEmpty()
					: FText::Format(LOCTEXT("Authority.SubText.RejectionFmt", "{0} accepted {1} rejected\nSee logs for details."),
						Request.TakeAuthority.Num() - Response.RejectedObjects.Num(),
						Response.RejectedObjects.Num()
						);
				
				ThisPin->AuthorityChangeNotification->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
				ThisPin->AuthorityChangeNotification->SetText(Text);
				ThisPin->AuthorityChangeNotification->SetSubText(SubText);

				if (!bSuccess)
				{
					const FString Error = FString::JoinBy(Response.RejectedObjects, TEXT("\n"), [](const TPair<FSoftObjectPath, FConcertStreamArray>& Pair)
					{
						return Pair.Key.ToString();
					});
					UE_LOG(LogConcert, Error, TEXT("Authority change rejected objects:\n%s"), *Error);
				}
				
				ThisPin->AuthorityChangeNotification->ExpireAndFadeout();
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE