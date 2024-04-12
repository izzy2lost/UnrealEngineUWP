// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Client/SHorizontalClientList.h"

#include "Widgets/Client/SClientName.h"

#include "Widgets/Client/SLocalClientName.h"
#include "Widgets/Client/SRemoteClientName.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHorizontalClientList"

namespace UE::ConcertClientSharedSlate
{
	namespace HorizontalClientList
	{
		TArray<FConcertSessionClientInfo> GetSortedClients(
			const TConstArrayView<FGuid>& Clients,
			const ConcertSharedSlate::FGetOptionalClientInfo& GetClientInfoDelegate,
			const SHorizontalClientList::FSortPredicate& SortPredicate
			)
		{
			// Prefetch the client info to avoid many GetClientInfoDelegate calls during Sort()
			TArray<FConcertSessionClientInfo> ClientsToDisplay;
			for (const FGuid& Client : Clients)
			{
				const TOptional<FConcertClientInfo> ClientInfo = GetClientInfoDelegate.Execute(Client);
				FConcertSessionClientInfo Info;
				Info.ClientEndpointId = Client;
				Info.ClientInfo = ClientInfo ? *ClientInfo : FConcertClientInfo{ .DisplayName = TEXT("Unavailable") };
				ClientsToDisplay.Add(Info);
			}

			ClientsToDisplay.Sort([&SortPredicate](const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right)
			{
				return SortPredicate.Execute(Left, Right);
			});
			return ClientsToDisplay;
		}
	}
	
	bool SHorizontalClientList::SortLocalClientFirstThenAlphabetical(const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right, ConcertSharedSlate::FIsLocalClient IsLocalClientDelegate)
	{
		// If one of the compare clients is local, always return that the local client is smaller.
		const bool bLeftIsLocalClient = IsLocalClientDelegate.IsBound() && IsLocalClientDelegate.Execute(Left.ClientEndpointId);
		const bool bRightIsLocalClient = IsLocalClientDelegate.IsBound() && IsLocalClientDelegate.Execute(Right.ClientEndpointId);
		return bLeftIsLocalClient
			|| (!bRightIsLocalClient && Left.ClientInfo.DisplayName < Right.ClientInfo.DisplayName);
	}

	TOptional<FString> SHorizontalClientList::GetDisplayString(
		const TConstArrayView<FGuid>& Clients,
		const ConcertSharedSlate::FGetOptionalClientInfo& GetClientInfoDelegate,
		const FSortPredicate& SortPredicate,
		const ConcertSharedSlate::FIsLocalClient& IsLocalClientDelegate
		)
	{
		const TArray<FConcertSessionClientInfo> ClientsToDisplay = HorizontalClientList::GetSortedClients(Clients, GetClientInfoDelegate, SortPredicate);
		if (!ClientsToDisplay.IsEmpty())
		{
			return {};
		}
		
		return FString::JoinBy(ClientsToDisplay, TEXT(", "), [&IsLocalClientDelegate](const FConcertSessionClientInfo& ClientInfo)
			{
				// GetSortedClients should return empty if GetSortedClients is invalid
				const bool bIsLocalClient = IsLocalClientDelegate.IsBound() && IsLocalClientDelegate.Execute(ClientInfo.ClientEndpointId);
				return SClientName::GetDisplayText(ClientInfo.ClientInfo, bIsLocalClient).ToString();
			});
	}

	void SHorizontalClientList::Construct(const FArguments& InArgs)
	{
		IsLocalClientDelegate = InArgs._IsLocalClient;
		GetClientInfoDelegate = InArgs._GetClientInfo;
		SortPredicateDelegate = InArgs._SortPredicate.IsBound()
			? InArgs._SortPredicate
			: FSortPredicate::CreateStatic(&SHorizontalClientList::SortLocalClientFirstThenAlphabetical, IsLocalClientDelegate);
		
		DisplayAvatarColorAttribute = InArgs._DisplayAvatarColor;
		HighlightTextAttribute = InArgs._HighlightText;
		
		NameFont = InArgs._Font;

		check(GetClientInfoDelegate.IsBound());
		
		ChildSlot
		[
			SAssignNew(WidgetSwitcher, SWidgetSwitcher)
			.WidgetIndex(0)
			+SWidgetSwitcher::Slot()
			[
				InArgs._EmptyListSlot.Widget
			]
			+SWidgetSwitcher::Slot()
			[
				SAssignNew(ScrollBox, SScrollBox)
				.Orientation(Orient_Horizontal)
				.ToolTipText(InArgs._ListToolTipText)
			]
		];
	}

	void SHorizontalClientList::RefreshList(const TConstArrayView<FGuid>& Clients)
	{
		ScrollBox->ClearChildren();

		if (Clients.IsEmpty())
		{
			WidgetSwitcher->SetActiveWidgetIndex(0);
			return;
		}
		WidgetSwitcher->SetActiveWidgetIndex(1);

		const TArray<FConcertSessionClientInfo> ClientsToDisplay = HorizontalClientList::GetSortedClients(Clients, GetClientInfoDelegate, SortPredicateDelegate);
		
		bool bIsFirst = true;
		for (const FConcertSessionClientInfo& Info : ClientsToDisplay)
		{
			if (!bIsFirst)
			{
				ScrollBox->AddSlot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(-1, 1, 0, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Comma", ", "))
					.Font(NameFont)
				];
			}

			const FGuid& EndpointId = Info.ClientEndpointId;
			const bool bIsLocalClient = IsLocalClientDelegate.IsBound() && IsLocalClientDelegate.Execute(Info.ClientEndpointId);
			if (bIsLocalClient)
			{
				ScrollBox->AddSlot()
				[
					SNew(SLocalClientName)
					.DisplayInfo_Lambda([this, EndpointId](){ return GetClientInfoDelegate.Execute(EndpointId); })
					.DisplayAvatarColor(DisplayAvatarColorAttribute)
					.HighlightText(HighlightTextAttribute)
					.Font(NameFont)
				];
			}
			else
			{
				ScrollBox->AddSlot()
				[
					SNew(SRemoteClientName)
					.DisplayInfo_Lambda([this, EndpointId](){ return GetClientInfoDelegate.Execute(EndpointId); })
					.DisplayAvatarColor(DisplayAvatarColorAttribute)
					.HighlightText(HighlightTextAttribute)
					.Font(NameFont)
				];
			}

			bIsFirst = false;
		}
	}
}

#undef LOCTEXT_NAMESPACE