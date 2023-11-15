// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssignPropertyComboBox.h"

#include "IConcertClient.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/ClientName/SHorizontalClientList.h"
#include "Widgets/ClientName/SLocalClientName.h"
#include "Widgets/ClientName/SRemoteClientName.h"

#include "Algo/AnyOf.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SAssignPropertyComboBox"

namespace UE::MultiUserClient
{
	void SAssignPropertyComboBox::Construct(const FArguments& InArgs,
        TSharedRef<ConcertClientSharedSlate::IMultiReplicationStreamEditor> InEditor,
        TSharedRef<IConcertClient> InConcertClient,
        FReplicationClientManager& InClientManager
	)
	{
		Editor = InEditor;
		ConcertClient = MoveTemp(InConcertClient);
		ClientManager = &InClientManager;
		
		Property = InArgs._DisplayedProperty;
		EditedObjects = InArgs._EditedObjects;
		HighlightText = InArgs._HighlightText;
		check(!EditedObjects.IsEmpty());
		
		ChildSlot
		[
			SNew(SComboButton)
			.HasDownArrow(true)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.ButtonContent()
			[
				SAssignNew(ClientListWidget, ConcertClientSharedSlate::SHorizontalClientList, ConcertClient.ToSharedRef())
				.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); })
				.EmptyListSlot()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("None", " - "))
				]
			]
			.OnGetMenuContent(this, &SAssignPropertyComboBox::GetMenuContent)
		];

		ClientManager->OnRemoteClientsChanged().AddSP(this, &SAssignPropertyComboBox::RebuildSubscriptionsAndRefresh);
		RebuildSubscriptions();
		RefreshContentBoxContent();
	}
	
	void SAssignPropertyComboBox::RefreshContentBoxContent()
	{
		TArray<FGuid> Clients;
		ClientManager->ForEachClient([this, &Clients](const FReplicationClient& Client)
		{
			const TMap<FSoftObjectPath, FReplicatedObjectInfo>& ObjectInfoMap = Client.GetStreamSynchronizer().GetServerState().ReplicatedObjects;
			for (const FSoftObjectPath& ObjectPath : EditedObjects)
			{
				if (const FReplicatedObjectInfo* ObjectInfo = ObjectInfoMap.Find(ObjectPath)
					; ObjectInfo && ObjectInfo->PropertySelection.ReplicatedProperties.Contains(Property))
				{
					Clients.Add(Client.GetEndpointId());
					return EBreakBehavior::Continue;
				}
			}
			return EBreakBehavior::Continue;
		});

		ClientListWidget->RefreshList(Clients);
	}

	TSharedRef<SWidget> SAssignPropertyComboBox::GetMenuContent()
	{
		using namespace ConcertClientSharedSlate;

		const auto MakeWidget = [this](const FGuid& EndpointId) -> TSharedRef<SWidget>
		{
			const bool bIsLocalClient = EndpointId == ConcertClient->GetCurrentSession()->GetSessionClientEndpointId();
			if (bIsLocalClient)
			{
				return SNew(SLocalClientName, ConcertClient.ToSharedRef())
					.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); });
			}
			return SNew(SRemoteClientName, ConcertClient.ToSharedRef())
				.ClientEndpointId(EndpointId)
				.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); });
		};
		
		FMenuBuilder MenuBuilder(true, nullptr);
		for (const FReplicationClient* Client : ClientUtils::GetSortedClientList(*ConcertClient, *ClientManager))
		{
			TAttribute<FText> Tooltip = TAttribute<FText>::CreateLambda([this, EndpointId = Client->GetEndpointId()]()
			{
				FText Reason;
				const bool bCanClick = CanClickOptionWithReason(EndpointId, &Reason);
				if (!bCanClick)
				{
					return Reason;
				}

				switch (GetOptionCheckState(EndpointId))
				{
				case ECheckBoxState::Unchecked: return LOCTEXT("Action.Unchecked", "Assign property to client and remove it from all others.");
				case ECheckBoxState::Undetermined:  return LOCTEXT("Action.Undetermined", "Assign property to client for all selected objects.");
				case ECheckBoxState::Checked: return LOCTEXT("Action.Checked", "Remove property from client and remove it from all others.");
				default: return FText::GetEmpty();
				}
			});
			
			MenuBuilder.AddMenuEntry(
				FUIAction(
					FExecuteAction::CreateSP(this, &SAssignPropertyComboBox::OnClickOption, Client->GetEndpointId()),
					FCanExecuteAction::CreateSP(this, &SAssignPropertyComboBox::CanClickOption, Client->GetEndpointId()),
					FGetActionCheckState::CreateSP(this, &SAssignPropertyComboBox::GetOptionCheckState, Client->GetEndpointId())
					),
				MakeWidget(Client->GetEndpointId()),
				NAME_None,
				Tooltip,
				EUserInterfaceActionType::ToggleButton
				);
		}
		
		return MenuBuilder.MakeWidget();
	}
	
	void SAssignPropertyComboBox::OnClickOption(const FGuid EndpointId) const
	{
		// Remote clients can disconnect after the combo-box is opened.
		const FReplicationClient* Client = ClientManager->FindClient(EndpointId);
		if (!Client)
		{
			return;
		}
		
		const FText TransactionText = FText::Format(LOCTEXT("AllClientsAssignFmt", "Assign {0} property"), FText::FromString(Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty)));
		FScopedTransaction Transaction(TransactionText);

		// Only one client is supposed to own the object: remove the other clients (if possible)
		ClientManager->ForEachClient([this, Client](const FReplicationClient& ClientToRemoveFrom)
		{
			if (*Client != ClientToRemoveFrom
				&& ClientToRemoveFrom.AllowsEditing())
			{
				for (const FSoftObjectPath& ObjectPath : EditedObjects)
				{
					ClientToRemoveFrom.GetClientEditModel()->RemoveProperties(ObjectPath, { Property });
				}
			}
			
			return EBreakBehavior::Continue;
		});

		const ECheckBoxState CheckBoxState = GetOptionCheckState(EndpointId);
		const bool bRemoveProperty = CheckBoxState == ECheckBoxState::Checked; 
		const TSharedRef<ConcertClientSharedSlate::IEditableReplicationStreamModel> EditModel = Client->GetClientEditModel();
		for (const FSoftObjectPath& ObjectPath : EditedObjects)
		{
			if (bRemoveProperty)
			{
				EditModel->RemoveProperties(ObjectPath, { Property });
				if (!EditModel->HasAnyPropertyAssigned(ObjectPath))
				{
					EditModel->RemoveObjects({ ObjectPath });
				}
			}
			else
			{
				if (!EditModel->ContainsObjects({ ObjectPath }))
				{
					EditModel->AddObjects({ ObjectPath.ResolveObject() });
				}
				
				if (EditModel->ContainsObjects({ ObjectPath }))
				{
					EditModel->AddProperties(ObjectPath, { Property });
				}
			}
		}
	}

#define SET_REASON(Text) if (Reason) { *Reason = Text; }
	bool SAssignPropertyComboBox::CanClickOptionWithReason(const FGuid& EndpointId, FText* Reason) const
	{
		const FReplicationClient* Client = ClientManager->FindClient(EndpointId);
		// Remote clients can disconnect after the combo-box is opened.
		if (!Client)
		{
			SET_REASON(LOCTEXT("ClientDisconnected", "Client disconnected."));
			return false;
		}

		// The combo box assigns the property to the clicked client and removes from the others... check that the currently assigned clients allow it.
		bool bCanRemoveFromOwners = true;
		ClientManager->ForEachClient([this, &EndpointId, &Reason, Client, &bCanRemoveFromOwners](const FReplicationClient& ClientToRemoveFrom)
		{
			if (*Client != ClientToRemoveFrom && !ClientToRemoveFrom.AllowsEditing())
			{
				const bool bHasAnySelectedObject = Algo::AnyOf(EditedObjects, [this, &ClientToRemoveFrom](const FSoftObjectPath& ObjectPath)
				{
					return ClientToRemoveFrom.GetClientEditModel()->HasProperty(ObjectPath, Property);
				});
				bCanRemoveFromOwners = !bHasAnySelectedObject;
				
				SET_REASON(FText::Format(
					LOCTEXT("OwningClientDoesNotAllow", "Client {0} does not allow remote editing of its properties but has registered this property."),
					FText::FromString(ClientUtils::GetClientDisplayName(*ConcertClient, EndpointId))
					));
				return EBreakBehavior::Break;
			}
			return EBreakBehavior::Continue;
		});

		if (!bCanRemoveFromOwners)
		{
			return false;
		}
		
		const bool bAllowsEditing = Client->AllowsEditing();
		if (!bAllowsEditing)
		{
			SET_REASON(FText::Format(
				LOCTEXT("RemoteEditingDisabled", "Client {0} does not allow remote editing of its properties."),
				FText::FromString(ClientUtils::GetClientDisplayName(*ConcertClient, EndpointId))
				));
		}
		return bAllowsEditing;
	}
#undef SET_REASON
	
	ECheckBoxState SAssignPropertyComboBox::GetOptionCheckState(const FGuid EndpointId) const
	{
		const FReplicationClient* Client = ClientManager->FindClient(EndpointId);
		// Remote clients can disconnect after the combo-box is opened.
		if (!Client)
		{
			return ECheckBoxState::Unchecked;
		}

		const TSharedRef<ConcertClientSharedSlate::IEditableReplicationStreamModel> Model = Client->GetClientEditModel();
		ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;
		for (const FSoftObjectPath& ObjectPath : EditedObjects)
		{
			const bool bHasProperty = Model->HasProperty(ObjectPath, Property);
			switch (CheckBoxState)
			{
			case ECheckBoxState::Unchecked:
				if (bHasProperty)
				{
					return ECheckBoxState::Undetermined;
				}
				break;
			case ECheckBoxState::Checked:
				if (!bHasProperty)
				{
					return ECheckBoxState::Undetermined;
				}
				break;
			case ECheckBoxState::Undetermined:
				CheckBoxState = bHasProperty ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				break;
			default: ;
			}
		}

		return CheckBoxState;
	}
	
	void SAssignPropertyComboBox::RebuildSubscriptions()
	{
		ClientManager->ForEachClient([this](FReplicationClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			Client.OnModelChanged().AddSP(this, &SAssignPropertyComboBox::RefreshContentBoxContent);
			return EBreakBehavior::Continue;
		});
	}
}

#undef LOCTEXT_NAMESPACE