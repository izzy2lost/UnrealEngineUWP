// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssignPropertyModel.h"

#include "ConcertLogGlobal.h"
#include "Replication/Client/Online/OnlineClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/PropertyUtils.h"

#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"

#include <type_traits>

#define LOCTEXT_NAMESPACE "FAssignPropertyModel"

namespace UE::MultiUserClient::Replication::MultiStreamColumns::AssignPropertyModel
{
	static void RemovePropertiesFromClient(
		const FOnlineClient& ClientToRemoveFrom,
		TConstArrayView<TSoftObjectPtr<>> Objects,
		const FConcertPropertyChain& Property
		)
	{
		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> EditModel = ClientToRemoveFrom.GetClientEditModel();
		for (const TSoftObjectPtr<>& Object : Objects)
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
	
	template<typename TShouldRemove>
	requires std::is_invocable_r_v<bool, TShouldRemove, const FOnlineClient&>
	static void UnassignPropertyFromClients(
		const FOnlineClientManager& ClientManager,
		TConstArrayView<TSoftObjectPtr<>> Objects,
		const FConcertPropertyChain& Property,
		TShouldRemove&& ShouldRemoveFromClient
		)
	{
		ClientManager.ForEachClient([Objects, Property, &ShouldRemoveFromClient](const FOnlineClient& ClientToRemoveFrom)
		{
			if (ShouldRemoveFromClient(ClientToRemoveFrom))
			{
				RemovePropertiesFromClient(ClientToRemoveFrom, Objects, Property);
			}
			return EBreakBehavior::Continue;
		});
	}
}

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	FAssignPropertyModel::FAssignPropertyModel(FOnlineClientManager& InClientManager)
		: ClientManager(InClientManager)
	{
		ClientManager.OnRemoteClientsChanged().AddRaw(this, &FAssignPropertyModel::BroadcastOnOwnershipChanged);
	}

	FAssignPropertyModel::~FAssignPropertyModel()
	{
		ClientManager.OnRemoteClientsChanged().RemoveAll(this);
	}

#define SET_REASON(Text) if (Reason) { *Reason = Text; }
	bool FAssignPropertyModel::CanChangePropertyFor(const FGuid& ClientId, FText* Reason) const
	{
		const FOnlineClient* Client = ClientManager.FindClient(ClientId);
		// Remote clients can disconnect after the combo-box is opened.
		if (!Client)
		{
			SET_REASON(LOCTEXT("ClientDisconnected", "Client disconnected."));
			return false;
		}
		
		return true;
	}
#undef SET_REASON

	bool FAssignPropertyModel::CanClear(TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property) const
	{
		bool bIsAssignedToAnyClient = false;
		ClientManager.ForEachClient([this, &Objects, &Property, &bIsAssignedToAnyClient](const FOnlineClient& Client)
		{
			for (const TSoftObjectPtr<>& EditedObject : Objects)
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

	EPropertyOnObjectsOwnershipState FAssignPropertyModel::GetPropertyOwnershipState(const FGuid& ClientId, TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property) const
	{
		const FOnlineClient* Client = ClientManager.FindClient(ClientId);
		// Remote clients can disconnect after the combo-box is opened.
		if (!Client)
		{
			return {};
		}

		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> Model = Client->GetClientEditModel();
		EPropertyOnObjectsOwnershipState Result = EPropertyOnObjectsOwnershipState::Mixed;
		for (const TSoftObjectPtr<>& ObjectPath : Objects)
		{
			const bool bHasProperty = Model->HasProperty(ObjectPath.GetUniqueID(), Property);
			const EPropertyOnObjectsOwnershipState ExpectedState = bHasProperty
				? EPropertyOnObjectsOwnershipState::OwnedOnAllObjects
				: EPropertyOnObjectsOwnershipState::NotOwnedOnAllObjects;
			
			if (Result == EPropertyOnObjectsOwnershipState::Mixed)
			{
				Result = ExpectedState;
				continue;
			}

			if (Result != ExpectedState)
			{
				return EPropertyOnObjectsOwnershipState::Mixed;
			}
		}
		return Result;
	}

	void FAssignPropertyModel::AssignPropertyTo(const FOnlineClient* Client, TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property)
	{
		const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> EditModel = Client->GetClientEditModel();
		for (const TSoftObjectPtr<>& Object : Objects)
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

	void FAssignPropertyModel::TogglePropertyFor(const FGuid& ClientId, TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property)
	{
		// Remote clients can disconnect after the combo-box is opened.
		const FOnlineClient* Client = ClientManager.FindClient(ClientId);
		if (!Client)
		{
			return;
		}
		const FText TransactionText = FText::Format(
			LOCTEXT("AllClientsAssignFmt", "Assign {0} property"),
			FText::FromString(Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty))
			);
		FScopedTransaction Transaction(TransactionText);

		const EPropertyOnObjectsOwnershipState OwnershipState = GetPropertyOwnershipState(ClientId, Objects, Property);
		const bool bRemovePropertyFromEditedClient = OwnershipState == EPropertyOnObjectsOwnershipState::OwnedOnAllObjects;
		
		// To make it simpler for the user, at most one client is supposed to be assigned to the object at any given time so ...
		if (bRemovePropertyFromEditedClient)
		{
			// ... remove property from all clients
			ClearProperty(Objects, Property);
		}
		else
		{
			// ... remove the property from all clients but the one we'll assign to ...
			AssignPropertyModel::UnassignPropertyFromClients(ClientManager, Objects, Property,
				[Client](const FOnlineClient& ClientToRemoveFrom){ return *Client != ClientToRemoveFrom; }
				);

			// ... and then assign the property
			AssignPropertyTo(Client, Objects, Property);
		}
	}

	void FAssignPropertyModel::ClearProperty(TConstArrayView<TSoftObjectPtr<>> Objects, const FConcertPropertyChain& Property) const
	{
		if (CanClear(Objects, Property))
		{
			const FText TransactionText = FText::Format(
				LOCTEXT("ClearAllClientsFmt", "Clear {0} property"),
				FText::FromString(Property.ToString(FConcertPropertyChain::EToStringMethod::LeafProperty))
				);
			FScopedTransaction Transaction(TransactionText);
			
			AssignPropertyModel::UnassignPropertyFromClients(ClientManager, Objects, Property, [](auto&){ return true; });
		}
	}
}

#undef LOCTEXT_NAMESPACE