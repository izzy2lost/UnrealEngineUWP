// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Async/Async.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Features/IModularFeatures.h"
#include "Misc/Guid.h"
#include "LiveLinkClient.h"
#include "LiveLinkHubClientsModel.h"
#include "LiveLinkHubUEClientInfo.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STreeView.h"

struct FGuid;
class FLiveLinkHub;

#define LOCTEXT_NAMESPACE "LiveLinkHub.ClientsView"

DECLARE_DELEGATE_OneParam(FOnClientSelected, FMessageAddress/*ClientIdentifier*/);

static const FName NameColumnId = "Name";
static const FName StatusColumnId = "Status";
static const FName EnabledIconColumnId = "EnabledIcon";

/** Tree view item that represents either a client or a livelink subject. */
struct FClientTreeViewItem
{
	virtual ~FClientTreeViewItem() = default;

	FClientTreeViewItem(FMessageAddress InClientAddress, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
		: ClientAddress(MoveTemp(InClientAddress))
		, ClientsModel(MoveTemp(InClientsModel))
	{
	}

	/** Get the subject key for this tree item (Invalid key for client rows). */
	virtual const FLiveLinkSubjectKey& GetSubjectKey() const
	{
		static const FLiveLinkSubjectKey InvalidSubjectKey;
		return InvalidSubjectKey;
	}

	/**
	 * For clients, returns if it should receive any livelink data from the hub (except for heartbeat messages).
	 * For subjects, returns if the hub transmit this subject's data to the client.
	 */
	virtual bool IsEnabled() const = 0;

	/** Whether the row should be in read only (ie. if the source is disconnected) */
	virtual bool IsReadOnly() const = 0;

	/**
	 * Set whether this item should be transmitted to the client. 
	 * @See IsEnabled()
	 */
	virtual void SetEnabled(bool bInEnabled) = 0;

	/** Get status text for the row. */
	virtual FText GetStatusText() const = 0;

	/** This item's children, in the case of client rows, these represent the livelink subjects. */
	TArray<TSharedPtr<FClientTreeViewItem>> Children;
	/** Name of the tree item (client or subject name). */
	FText Name;
	/** Identifier of the unreal client for this item. */
	FMessageAddress ClientAddress;
	/** ClientsModel used to retrieve information about clients/subjects. */
	TWeakPtr<ILiveLinkHubClientsModel> ClientsModel;
};

/** Holds a client row's data. */
struct FClientTreeViewClientItem : public FClientTreeViewItem
{
	FClientTreeViewClientItem(FMessageAddress InClientAddress, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
		: FClientTreeViewItem(MoveTemp(InClientAddress), MoveTemp(InClientsModel))
	{
		if (TSharedPtr<ILiveLinkHubClientsModel> ClientModelPtr = ClientsModel.Pin())
		{
			if (TOptional<FLiveLinkHubUEClientInfo> ClientInfo = ClientModelPtr->GetClientInfo(ClientAddress))
			{
				Name = ClientInfo ? FText::FromString(ClientInfo->LongName) : LOCTEXT("InvalidSourceLabel", "Invalid Source");
			}
			else
			{
				ensureMsgf(false, TEXT("Client Info was invalid"));
			}
		}
	}

	virtual bool IsEnabled() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return ClientsModelPtr->IsClientEnabled(ClientAddress);
		}
		return false;
	}

	virtual bool IsReadOnly() const override
	{
		// todo: when the client list will be modified to show disconnected sources, put this in read only when client is disconnected.
		return false;
	}

	virtual void SetEnabled(bool bInEnabled) override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			ClientsModelPtr->SetClientEnabled(ClientAddress, bInEnabled);
		}
	}

	virtual FText GetStatusText() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return ClientsModelPtr->GetClientStatus(ClientAddress);
		}

		return LOCTEXT("InvalidStatus", "Invalid");
	}
	//~ End FClientTreeViewItem interface
};

/** Holds a subject row's data. */
struct FClientTreeViewSubjectItem : public FClientTreeViewItem
{
	FClientTreeViewSubjectItem(FMessageAddress InClientAddress, FLiveLinkSubjectKey InLiveLinkSubjectKey, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
		: FClientTreeViewItem(MoveTemp(InClientAddress), MoveTemp(InClientsModel))
		, LiveLinkSubjectKey(MoveTemp(InLiveLinkSubjectKey))
	{
		const FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		Name = FText::Format(LOCTEXT("SubjectName", "{0} - {1}"), FText::FromName(LiveLinkSubjectKey.SubjectName), LiveLinkClient.GetSourceType(LiveLinkSubjectKey.Source));
	}

	//~ Begin FClientTreeViewItem interface
	virtual const FLiveLinkSubjectKey& GetSubjectKey() const override
	{
		return LiveLinkSubjectKey;
	}

	virtual bool IsEnabled() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return ClientsModelPtr->IsSubjectEnabled(ClientAddress, LiveLinkSubjectKey);
		}
		return false;
	}

	virtual bool IsReadOnly() const override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			return !ClientsModelPtr->IsClientEnabled(ClientAddress);
		}

		return false;
	}

	virtual void SetEnabled(bool bInEnabled) override
	{
		if (const TSharedPtr<ILiveLinkHubClientsModel> ClientsModelPtr = ClientsModel.Pin())
		{
			ClientsModelPtr->SetSubjectEnabled(ClientAddress, LiveLinkSubjectKey, bInEnabled);
		}
	}

	virtual FText GetStatusText() const override
	{
		return FText::GetEmpty();
	}
	//~ End FClientTreeViewItem interface

	/** Unique key for this item's subject. */
	FLiveLinkSubjectKey LiveLinkSubjectKey;
};


class SLiveLinkHubClientsRow : public SMultiColumnTableRow<TSharedPtr<FClientTreeViewItem>>
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubClientsRow) {}
		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FClientTreeViewItem>, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		TreeItem = InArgs._Item;

		SMultiColumnTableRow<TSharedPtr<FClientTreeViewItem>>::Construct(
			FSuperRowType::FArguments()
			.Padding(1.0f),
			InOwnerTableView
		);
	}

	//~ Begin SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		if (ColumnName == NameColumnId)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6, 0, 0, 0)
				[
					SNew(SExpanderArrow, SharedThis(this)).IndentAmount(12)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.ToolTipText(TreeItem->Name)
					.Text(TreeItem->Name)
				];
		}
		else if (ColumnName == StatusColumnId)
		{
			return SNew(STextBlock)
				.Text(this, &SLiveLinkHubClientsRow::GetStatusText);
		}
		else if (ColumnName == EnabledIconColumnId)
		{
			return SNew(SCheckBox)
				.IsChecked(MakeAttributeSP(this, &SLiveLinkHubClientsRow::IsItemEnabled))
				.IsEnabled(this, &SLiveLinkHubClientsRow::IsCheckboxEnabled)
				.OnCheckStateChanged(this, &SLiveLinkHubClientsRow::OnEnabledCheckboxChange);
		}

		return SNullWidget::NullWidget;
	}
	//~ End SMultiColumnTableRow interface

private:
	/** Return whether the enable checkbox should be clickable. */
	bool IsCheckboxEnabled() const
	{
		return !TreeItem->IsReadOnly();
	}

	/** Return whether the enabled checkbox is checked. */
	ECheckBoxState IsItemEnabled() const
	{
		return TreeItem->IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Handler called when the enabled checkbox is clicked. */
	void OnEnabledCheckboxChange(ECheckBoxState State) const
	{
		TreeItem->SetEnabled(State == ECheckBoxState::Checked);
	}

	/** Get the status text from the tree item. */
	FText GetStatusText() const
	{
		return TreeItem->GetStatusText();
	}

private:
	/** The data represented by this row (Either an unreal client or a livelink subject). */
	TSharedPtr<FClientTreeViewItem> TreeItem;
};

/**
 * Provides the UI that displays the UE clients connected to the hub. 
 */
class SLiveLinkHubClientsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubClientsView) {}
	SLATE_EVENT(FOnClientSelected, OnClientSelected)
	SLATE_END_ARGS()

	using FClientTreeItemPtr = TSharedPtr<FClientTreeViewItem>;

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs, TSharedRef<ILiveLinkHubClientsModel> InClientsModel)
	{
		OnClientSelectedDelegate = InArgs._OnClientSelected;
		ClientsModel = MoveTemp(InClientsModel);
		 
		ClientsModel->OnClientEvent().AddSP(this, &SLiveLinkHubClientsView::OnClientEvent);

		FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		LiveLinkClient.OnLiveLinkSubjectAdded().AddSP(this, &SLiveLinkHubClientsView::OnSubjectAdded_AnyThread);
		LiveLinkClient.OnLiveLinkSubjectRemoved().AddSP(this, &SLiveLinkHubClientsView::OnSubjectRemoved_AnyThread);

		ChildSlot
		[
			SAssignNew(TreeView, STreeView<FClientTreeItemPtr>)
				.TreeItemsSource(&Clients)
				.ItemHeight(20.0f)
				.OnSelectionChanged(this, &SLiveLinkHubClientsView::OnSelectionChanged)
				.OnGenerateRow(this, &SLiveLinkHubClientsView::OnGenerateClientRow)
				.OnGetChildren(this, &SLiveLinkHubClientsView::OnGetChildren)
				.HeaderRow
				(
					SNew(SHeaderRow)
					+ SHeaderRow::Column(NameColumnId)
					.FillWidth(0.75f)
					.DefaultLabel(LOCTEXT("ItemName", "Name"))
					+ SHeaderRow::Column(StatusColumnId)
					.DefaultLabel(LOCTEXT("Status", "Status"))
					.FillWidth(0.25f)
					+ SHeaderRow::Column(EnabledIconColumnId)
					.ManualWidth(20.f)
					.DefaultLabel(LOCTEXT("EnabledIconEmpty", ""))
				)
		];

		PopulateClients();
	}
	//~ End SWidget interface

	virtual ~SLiveLinkHubClientsView() override
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			LiveLinkClient.OnLiveLinkSubjectRemoved().RemoveAll(this);
			LiveLinkClient.OnLiveLinkSubjectAdded().RemoveAll(this);
		}

		if (ClientsModel)
		{
			ClientsModel->OnClientEvent().RemoveAll(this);
		}
	}

	/** Get the currently selected client, an invalid address if none is currently selected. */
	FMessageAddress GetSelectedClient() const
	{
		TArray<FClientTreeItemPtr> SelectedClients = TreeView->GetSelectedItems();
		if (SelectedClients.Num())
		{
			return SelectedClients[0]->ClientAddress;
		}

		return FMessageAddress();
	}

private:
	/** Handler used to generate a widget for a given client row. */
	TSharedRef<ITableRow> OnGenerateClientRow(FClientTreeItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SLiveLinkHubClientsRow, OwnerTable)
			.Item(Item);
	}

	/** Handler called to fetch a tree row's children. */
	void OnGetChildren(FClientTreeItemPtr Item, TArray<FClientTreeItemPtr>& OutChildren)
	{
		OutChildren.Append(Item->Children);
	}

	/** Handler called when selection changes in the list view. */
	void OnSelectionChanged(FClientTreeItemPtr InItem, const ESelectInfo::Type InSelectInfoType)
	{
		if (InItem)
		{
			OnClientSelectedDelegate.ExecuteIfBound(InItem->ClientAddress);
		}
	}

	/** Handler called when the client list has changed. */
	void OnClientEvent(FMessageAddress MessageAddress, ILiveLinkHubClientsModel::EClientEventType EventType)
	{
		switch (EventType)
		{
		case ILiveLinkHubClientsModel::EClientEventType::Connected:
		{
			if (!Clients.ContainsByPredicate([MessageAddress](const FClientTreeItemPtr& InClient) { return InClient->ClientAddress == MessageAddress; }))
			{
				TSharedPtr<FClientTreeViewClientItem> ClientItem = MakeShared<FClientTreeViewClientItem>(MessageAddress, ClientsModel.ToSharedRef());
				ClientItem->ClientAddress = MessageAddress;
				InitializeClientItem(*ClientItem);

				Clients.Add(ClientItem);
				TreeView->RequestTreeRefresh();
			}
			break;
		}
		case ILiveLinkHubClientsModel::EClientEventType::Disconnected:
		{
			Clients.RemoveAll([MessageAddress](const FClientTreeItemPtr& InClient) { return InClient->ClientAddress == MessageAddress; });
			TreeView->RequestTreeRefresh();
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

	/** Populate a client item row with its data and children. */
	void InitializeClientItem(FClientTreeViewClientItem& ClientItem)
	{
		const FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		constexpr bool bIncludeDisabledSubject = true;
		constexpr bool bIncludeVirtualSubject = true;

		TArray<FLiveLinkSubjectKey> LiveLinkSubjects = LiveLinkClient.GetSubjects(bIncludeDisabledSubject, bIncludeVirtualSubject);
		ClientItem.Children.Reserve(LiveLinkSubjects.Num());

		for (const FLiveLinkSubjectKey& SubjectKey : LiveLinkSubjects)
		{
			TSharedPtr<FClientTreeViewSubjectItem> SubjectItem = MakeShared<FClientTreeViewSubjectItem>(ClientItem.ClientAddress, SubjectKey, ClientsModel.ToSharedRef());
			ClientItem.Children.Add(SubjectItem);
		}
	}

	/** AnyThread handler for the SubjectAdded delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectAdded_AnyThread(FLiveLinkSubjectKey SubjectKey)
	{
		TWeakPtr<SLiveLinkHubClientsView> Self = StaticCastSharedRef<SLiveLinkHubClientsView>(AsShared());
		AsyncTask(ENamedThreads::GameThread, [Self, Key = MoveTemp(SubjectKey)]
		{
			if (TSharedPtr<SLiveLinkHubClientsView> View = Self.Pin())
			{
				View->OnSubjectAdded(Key);
			}
		});
	}

	/** Handles updating the tree view when a subject is added. */
	void OnSubjectAdded(const FLiveLinkSubjectKey& SubjectKey)
	{
		for (const FClientTreeItemPtr& Client : Clients)
		{
			TSharedPtr<FClientTreeViewSubjectItem> SubjectItem = MakeShared<FClientTreeViewSubjectItem>(Client->ClientAddress, SubjectKey, ClientsModel.ToSharedRef());
			SubjectItem->LiveLinkSubjectKey = SubjectKey;

			if (!Client->Children.ContainsByPredicate([&](const TSharedPtr<FClientTreeViewItem>& Child)
			{
				return Child->GetSubjectKey() == SubjectKey;
			}))
			{
				Client->Children.Add(SubjectItem);
			}
		}

		TreeView->RequestTreeRefresh();
	}

	/** AnyThread handler for the SubjectRemoved delegate, dispatches handling on the game thread to avoid asserts in Slate. */
	void OnSubjectRemoved_AnyThread(FLiveLinkSubjectKey SubjectKey)
	{
		TWeakPtr<SLiveLinkHubClientsView> Self = StaticCastSharedRef<SLiveLinkHubClientsView>(AsShared());
		AsyncTask(ENamedThreads::GameThread, [Self, Key = MoveTemp(SubjectKey)]
		{
			if (TSharedPtr<SLiveLinkHubClientsView> View = Self.Pin())
			{
				View->OnSubjectRemoved(Key);
			}
		});
	}

	/** Handles updating the tree view when a subject is removed. */
	void OnSubjectRemoved(const FLiveLinkSubjectKey& SubjectKey)
	{
		for (const FClientTreeItemPtr& Client : Clients)
		{
			int32 Index = Client->Children.IndexOfByPredicate([SubjectKey](const TSharedPtr<FClientTreeViewItem>& Item) { return Item->GetSubjectKey() == SubjectKey; });
			if (Index != INDEX_NONE)
			{
				Client->Children.RemoveAt(Index);
			}
		}

		TreeView->RequestTreeRefresh();
	}

	/** Build the client list. */
	void PopulateClients()
	{
		TArray<FMessageAddress> ClientList = ClientsModel->GetClients();
		Clients.Reset(ClientList.Num());

		for (FMessageAddress Client : ClientList)
		{
			TSharedPtr<FClientTreeViewClientItem> ClientItem = MakeShared<FClientTreeViewClientItem>(Client, ClientsModel.ToSharedRef());
			ClientItem->ClientAddress = Client;
			InitializeClientItem(*ClientItem);
			Clients.Add(ClientItem);
		}
	}

private:
	/** Delegate called when a client is selected. */
	FOnClientSelected OnClientSelectedDelegate;
	/** TreeView widget that displays the clients. */
	TSharedPtr<STreeView<FClientTreeItemPtr>> TreeView;
	/** List of message addresses used to identify UE clients and populate the list view. */
	TArray<FClientTreeItemPtr> Clients;
	/** Model that holds the client data we are displaying. */
	TSharedPtr<ILiveLinkHubClientsModel> ClientsModel;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.ClientsView */
