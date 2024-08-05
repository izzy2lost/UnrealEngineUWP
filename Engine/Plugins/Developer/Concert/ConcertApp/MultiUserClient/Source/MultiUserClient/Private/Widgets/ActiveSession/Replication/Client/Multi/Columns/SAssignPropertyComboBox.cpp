// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssignPropertyComboBox.h"

#include "ConcertLogGlobal.h"
#include "IConcertClient.h"
#include "Replication/Client/Online/OnlineClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/PropertyUtils.h"
#include "Widgets/ActiveSession/Replication/Misc/SNoClients.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/Client/ClientInfoHelpers.h"
#include "Widgets/Client/SHorizontalClientList.h"
#include "Widgets/Client/SLocalClientName.h"
#include "Widgets/Client/SRemoteClientName.h"

#include "Algo/AnyOf.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SAssignPropertyComboBox"

namespace UE::MultiUserClient::Replication
{
	namespace AssignPropertyComboBox
	{
		TArray<FGuid> GetDisplayedClients(const FOnlineClientManager& ClientManager, const FConcertPropertyChain& DisplayedProperty, const TArray<TSoftObjectPtr<>>& EditedObjects)
		{
			TArray<FGuid> Clients;
			ClientManager.ForEachClient([&DisplayedProperty, &EditedObjects, &Clients](const FOnlineClient& Client)
			{
				const TMap<FSoftObjectPath, FConcertReplicatedObjectInfo>& ObjectInfoMap = Client.GetStreamSynchronizer().GetServerState().ReplicatedObjects;
				for (const TSoftObjectPtr<>& ObjectPath : EditedObjects)
				{
					if (const FConcertReplicatedObjectInfo* ObjectInfo = ObjectInfoMap.Find(ObjectPath.GetUniqueID())
						; ObjectInfo && ObjectInfo->PropertySelection.ReplicatedProperties.Contains(DisplayedProperty))
					{
						Clients.Add(Client.GetEndpointId());
						return EBreakBehavior::Continue;
					}
				}
				return EBreakBehavior::Continue;
			});
			return Clients;
		}
	}
	
	TOptional<FString> SAssignPropertyComboBox::GetDisplayString(
		const TSharedRef<IConcertClient>& LocalConcertClient,
		const FOnlineClientManager& ClientManager,
		const FConcertPropertyChain& DisplayedProperty,
		const TArray<TSoftObjectPtr<>>& EditedObjects)
	{
		using SWidgetType = ConcertClientSharedSlate::SHorizontalClientList;
		const TArray<FGuid> Clients = AssignPropertyComboBox::GetDisplayedClients(ClientManager, DisplayedProperty, EditedObjects);
		const ConcertSharedSlate::FIsLocalClient IsLocalClientDelegate = ConcertClientSharedSlate::MakeIsLocalClientGetter(LocalConcertClient);
		return SWidgetType::GetDisplayString(
			Clients,
			ConcertClientSharedSlate::MakeClientInfoGetter(LocalConcertClient),
			SWidgetType::FSortPredicate::CreateStatic(&SWidgetType::SortLocalClientFirstThenAlphabetical, IsLocalClientDelegate),
			IsLocalClientDelegate
			);
	}

	void SAssignPropertyComboBox::Construct(const FArguments& InArgs,
	    TSharedRef<ConcertSharedSlate::IMultiReplicationStreamEditor> InEditor,
	    TSharedRef<IConcertClient> InConcertClient,
	    FOnlineClientManager& InClientManager
	)
	{
		Editor = MoveTemp(InEditor);
		ConcertClient = MoveTemp(InConcertClient);
		ClientManager = &InClientManager;
		
		Property = InArgs._DisplayedProperty;
		EditedObjects = InArgs._EditedObjects;
		HighlightText = InArgs._HighlightText;
		check(!EditedObjects.IsEmpty());

		OnOptionClickedDelegate = InArgs._OnPropertyAssignmentChanged;
		
		ChildSlot
		[
			SNew(SComboButton)
			.HasDownArrow(true)
			.ButtonContent()
			[
				SAssignNew(ClientListWidget, ConcertClientSharedSlate::SHorizontalClientList)
				.IsLocalClient(ConcertClientSharedSlate::MakeIsLocalClientGetter(ConcertClient.ToSharedRef()))
				.GetClientInfo(ConcertClientSharedSlate::MakeClientInfoGetter(ConcertClient.ToSharedRef()))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); })
				.EmptyListSlot() [ SNew(SNoClients) ]
			]
			.OnGetMenuContent(this, &SAssignPropertyComboBox::GetMenuContent)
		];

		ClientManager->OnRemoteClientsChanged().AddSP(this, &SAssignPropertyComboBox::RebuildSubscriptionsAndRefresh);
		RebuildSubscriptions();
		RefreshContentBoxContent();
	}
	
	void SAssignPropertyComboBox::RefreshContentBoxContent() const
	{
		ClientListWidget->RefreshList(
			AssignPropertyComboBox::GetDisplayedClients(*ClientManager, Property, EditedObjects)
			);
	}

	TSharedRef<SWidget> SAssignPropertyComboBox::GetMenuContent()
	{
		using namespace ConcertClientSharedSlate;

		const auto MakeWidget = [this](const FGuid& EndpointId) -> TSharedRef<SWidget>
		{
			const bool bIsLocalClient = EndpointId == ConcertClient->GetCurrentSession()->GetSessionClientEndpointId();
			if (bIsLocalClient)
			{
				return SNew(SLocalClientName)
					.DisplayInfo(ConcertClientSharedSlate::MakeLocalClientInfoAttribute(ConcertClient.ToSharedRef()))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); });
			}
			return SNew(SRemoteClientName)
				.DisplayInfo(ConcertClientSharedSlate::MakeClientInfoAttribute(ConcertClient.ToSharedRef(), EndpointId))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); });
		};
		
		FMenuBuilder MenuBuilder(true, nullptr);
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Clear.Label", "Clear"),
			LOCTEXT("Clear.Tooltip", "Stop this property from being replicated"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssignPropertyComboBox::OnClickClear),
				FCanExecuteAction::CreateSP(this, &SAssignPropertyComboBox::CanClickClear)
				),
			NAME_None,
			EUserInterfaceActionType::Button
		);
		
		MenuBuilder.BeginSection(TEXT("AssignTo"), LOCTEXT("AssignTo", "Assign to"));
		for (const FOnlineClient* Client : ClientUtils::GetSortedClientList(*ConcertClient, *ClientManager))
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
				EUserInterfaceActionType::Check
				);
		}
		MenuBuilder.EndSection();
		
		return MenuBuilder.MakeWidget();
	}
	
	void SAssignPropertyComboBox::OnClickOption(const FGuid EndpointId) const
	{
		// Remote clients can disconnect after the combo-box is opened.
		const FOnlineClient* Client = ClientManager->FindClient(EndpointId);
		if (!Client)
		{
			return;
		}
		
		const FText TransactionText = FText::Format(LOCTEXT("AllClientsAssignFmt", "Assign {0} property"), FText::FromString(Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty)));
		FScopedTransaction Transaction(TransactionText);

		const ECheckBoxState CheckBoxState = GetOptionCheckState(EndpointId);
		const bool bRemovePropertyFromEditedClient = CheckBoxState == ECheckBoxState::Checked;
		
		// To make it simpler for the user, at most one client is supposed to be assigned to the object at any given time so ...
		if (bRemovePropertyFromEditedClient)
		{
			// ... remove property from all clients
			UnassignPropertyFromClients([](const FOnlineClient& ClientToRemoveFrom){ return true; });
		}
		else
		{
			// ... remove the property from all clients but the one we'll assign to ...
			UnassignPropertyFromClients([Client](const FOnlineClient& ClientToRemoveFrom){ return *Client != ClientToRemoveFrom; });

			// ... and then assign the property
			const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> EditModel = Client->GetClientEditModel();
			for (const TSoftObjectPtr<>& Object : EditedObjects)
			{
				const FSoftObjectPath& ObjectPath = Object.GetUniqueID();
				if (!EditModel->ContainsObjects({ ObjectPath }))
				{
					EditModel->AddObjects({ Object.Get() });
				}

				const FSoftClassPath ClassPath = EditModel->GetObjectClass(ObjectPath);
				TArray AddedProperties { Property };
				ConcertClientSharedSlate::PropertyUtils::AppendAdditionalPropertiesToAdd(ClassPath, AddedProperties);
				EditModel->AddProperties(ObjectPath, AddedProperties);
			}
		}
		
		OnOptionClickedDelegate.ExecuteIfBound();
	}

#define SET_REASON(Text) if (Reason) { *Reason = Text; }
	bool SAssignPropertyComboBox::CanClickOptionWithReason(const FGuid& EndpointId, FText* Reason) const
	{
		const FOnlineClient* Client = ClientManager->FindClient(EndpointId);
		// Remote clients can disconnect after the combo-box is opened.
		if (!Client)
		{
			SET_REASON(LOCTEXT("ClientDisconnected", "Client disconnected."));
			return false;
		}

		// The combo box assigns the property to the clicked client and removes from the others... check that the currently assigned clients allow it.
		bool bCanRemoveFromOwners = true;
		ClientManager->ForEachClient([this, &EndpointId, &Reason, Client, &bCanRemoveFromOwners](const FOnlineClient& ClientToRemoveFrom)
		{
			if (*Client != ClientToRemoveFrom && !ClientToRemoveFrom.AllowsEditing())
			{
				const bool bHasAnySelectedObject = Algo::AnyOf(EditedObjects, [this, &ClientToRemoveFrom](const TSoftObjectPtr<>& Object)
				{
					return ClientToRemoveFrom.GetClientEditModel()->HasProperty(Object.GetUniqueID(), Property);
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
		const FOnlineClient* Client = ClientManager->FindClient(EndpointId);
		// Remote clients can disconnect after the combo-box is opened.
		if (!Client)
		{
			return ECheckBoxState::Unchecked;
		}

		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> Model = Client->GetClientEditModel();
		ECheckBoxState CheckBoxState = ECheckBoxState::Undetermined;
		for (const TSoftObjectPtr<>& ObjectPath : EditedObjects)
		{
			const bool bHasProperty = Model->HasProperty(ObjectPath.GetUniqueID(), Property);
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

	void SAssignPropertyComboBox::OnClickClear()
	{
		const FText TransactionText = FText::Format(LOCTEXT("ClearAllClientsFmt", "Clear {0} property"), FText::FromString(Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty)));
		FScopedTransaction Transaction(TransactionText);

		UnassignPropertyFromClients([](const FOnlineClient& ClientToRemoveFrom){ return true; });
		OnOptionClickedDelegate.ExecuteIfBound();
	}

	bool SAssignPropertyComboBox::CanClickClear() const
	{
		bool bIsAssignedToAnyClient = false;
		ClientManager->ForEachClient([this, &bIsAssignedToAnyClient](const FOnlineClient& Client)
		{
			for (const TSoftObjectPtr<>& EditedObject : EditedObjects)
			{
				if (bIsAssignedToAnyClient)
				{
					break;
				}
				
				const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> Model = Client.GetClientEditModel();
				const bool bHasProperty = Model->HasProperty(EditedObject.GetUniqueID(), Property);
				bIsAssignedToAnyClient |= bHasProperty;
			}
			
			return bIsAssignedToAnyClient ? EBreakBehavior::Break : EBreakBehavior::Continue;
		});
		return bIsAssignedToAnyClient;
	}

	void SAssignPropertyComboBox::UnassignPropertyFromClients(TFunctionRef<bool(const FOnlineClient& Client)> ShouldRemoveFromClient) const
	{
		ClientManager->ForEachClient([this, &ShouldRemoveFromClient](const FOnlineClient& ClientToRemoveFrom)
		{
			const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> EditModel = ClientToRemoveFrom.GetClientEditModel();
			if (ClientToRemoveFrom.AllowsEditing() && ShouldRemoveFromClient(ClientToRemoveFrom))
			{
				for (const TSoftObjectPtr<>& Object : EditedObjects)
				{
					const FSoftObjectPath& ObjectPath = Object.GetUniqueID();
					const FSoftClassPath ClassPath = EditModel->GetObjectClass(ObjectPath);
					EditModel->RemoveProperties(ObjectPath, { Property });
					
					if (EditModel->HasAnyPropertyAssigned(ObjectPath))
					{
						continue;
					}

					// We want to remove subobjects that have no properties. Retain actors because they cause their entire component / subobject hierarchy to be displayed.
					// Skipping this check would close the entire property tree view and remove the actor hierarchy from the view.
					// That would feel very unnatural / unexpected for the user. 
					// If the user does not want the actor anymore, they should click it and delete it.
					const UClass* ObjectClass = ClassPath.IsValid() ? ClassPath.TryLoadClass<UObject>() : nullptr;
					UE_CLOG(ClassPath.IsValid() && !ObjectClass, LogConcert, Warning, TEXT("SAssignPropertyComboBox: Failed to resolve class %s"), *ClassPath.ToString());
					const bool bIsTopLevelObject = ObjectClass && !ObjectClass->IsChildOf<AActor>();
					if (bIsTopLevelObject)
					{
						EditModel->RemoveObjects({ ObjectPath });
					}
				}
			}
			
			return EBreakBehavior::Continue;
		});
	}

	void SAssignPropertyComboBox::RebuildSubscriptions()
	{
		ClientManager->ForEachClient([this](FOnlineClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			Client.OnModelChanged().AddSP(this, &SAssignPropertyComboBox::RefreshContentBoxContent);
			return EBreakBehavior::Continue;
		});
	}
}

#undef LOCTEXT_NAMESPACE