// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Async/Async.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Misc/Guid.h"
#include "LiveLinkHubClientsModel.h"
#include "LiveLinkHubModule.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

struct FGuid;
class FLiveLinkHub;

#define LOCTEXT_NAMESPACE "LiveLinkHub.ClientsView"

DECLARE_DELEGATE_OneParam(FOnClientSelected, const TSharedPtr<FMessageAddress>&/*ClientIdentifier*/);

/**
 * Provides the UI that displays the UE clients connected to the hub. 
 */
class SLiveLinkHubClientsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubClientsView) {}
	SLATE_EVENT(FOnClientSelected, OnClientSelected)
	SLATE_END_ARGS()

	using FClientIdentifierPtr = TSharedPtr<FMessageAddress>;

	/**
	* @param InArgs
	*/
	void Construct(const FArguments& InArgs, TSharedPtr<ILiveLinkHubClientsModel> InClientsModel)
	{
		OnClientSelectedDelegate = InArgs._OnClientSelected;
		ClientsModel = MoveTemp(InClientsModel);
		 
		ClientsModel->OnClientEvent().AddSP(this, &SLiveLinkHubClientsView::OnClientEvent);

		ChildSlot
			[
				SAssignNew(ListView, SListView<FClientIdentifierPtr>)
					.ListItemsSource(&Clients)
					.OnSelectionChanged(this, &SLiveLinkHubClientsView::OnSelectionChanged)
					.OnGenerateRow(this, &SLiveLinkHubClientsView::OnGenerateClientRow)
			];
	}

	virtual ~SLiveLinkHubClientsView()
	{
		if (ClientsModel)
		{
			ClientsModel->OnClientEvent().RemoveAll(this);
		}
	}

	/** Get the currently selected client, or nullptr if none is currently selected. */
	FClientIdentifierPtr GetSelectedClient() const
	{
		TArray<FClientIdentifierPtr> SelectedClients = ListView->GetSelectedItems();
		return SelectedClients.Num() ? SelectedClients[0] : nullptr;
	}
	
	/** Handler used to generate a widget for a given client row. */
	TSharedRef<ITableRow> OnGenerateClientRow(FClientIdentifierPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		TOptional<FLiveLinkHubUEClientInfo> ClientInfo = ClientsModel->GetClientInfo(*Item);
		const FText Representation = ClientInfo ? FText::FromString(ClientInfo->LongName) : LOCTEXT("InvalidSourceLabel", "Invalid Source");
		
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
		.Content()
		[
			SNew(STextBlock)
			.Text(Representation)
		];
	}

private:
	/** Handler called when selection changes in the list view. */
	void OnSelectionChanged(FClientIdentifierPtr InClientInfo, const ESelectInfo::Type InSelectInfoType)
	{
		OnClientSelectedDelegate.ExecuteIfBound(InClientInfo);
	}

	/** Handler called when the client list has changed. */
	void OnClientEvent(const FMessageAddress& MessageAddress, ILiveLinkHubClientsModel::EClientEventType EventType)
	{
		switch (EventType)
		{
		case ILiveLinkHubClientsModel::EClientEventType::Connected:
		{
			if (!Clients.ContainsByPredicate([MessageAddress](const FClientIdentifierPtr& InClient) { return *InClient == MessageAddress; }))
			{
				Clients.Add(MakeShared<FMessageAddress>(MessageAddress));
				ListView->RequestListRefresh();
			}
			break;
		}
		case ILiveLinkHubClientsModel::EClientEventType::Disconnected:
		{
			Clients.RemoveAll([MessageAddress](const FClientIdentifierPtr& InClient) { return *InClient == MessageAddress; });
			ListView->RequestListRefresh();
			break;
		}
		case ILiveLinkHubClientsModel::EClientEventType::Modified:
		{
			break;
		}
		default:
		{
			checkNoEntry();
		}
		}
	}
private:
	/** Delegate called when a client is selected. */
	FOnClientSelected OnClientSelectedDelegate;
	/** ListView widget that displays the clients. */
	TSharedPtr<SListView<FClientIdentifierPtr>> ListView;
	/** List of message addresses used to identify UE clients and populate the list view. */
	TArray<FClientIdentifierPtr> Clients;
	/** Model that holds the client data we are displaying. */
	TSharedPtr<ILiveLinkHubClientsModel> ClientsModel;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.ClientsView */