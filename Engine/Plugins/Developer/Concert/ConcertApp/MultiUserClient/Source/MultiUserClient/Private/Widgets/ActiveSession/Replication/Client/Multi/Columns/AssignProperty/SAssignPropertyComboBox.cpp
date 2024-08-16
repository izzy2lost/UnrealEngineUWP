// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAssignPropertyComboBox.h"

#include "AssignPropertyModel.h"
#include "IConcertClient.h"
#include "Replication/Client/Online/OnlineClient.h"
#include "Replication/Client/Online/OnlineClientManager.h"
#include "Replication/Editor/Model/PropertyUtils.h"
#include "Widgets/ActiveSession/Replication/Misc/SNoClients.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/Client/ClientInfoHelpers.h"
#include "Widgets/Client/SHorizontalClientList.h"
#include "Widgets/Client/SLocalClientName.h"
#include "Widgets/Client/SRemoteClientName.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "SAssignPropertyComboBox"

namespace UE::MultiUserClient::Replication::MultiStreamColumns
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
		using SWidgetType = ConcertSharedSlate::SHorizontalClientList;
		const TArray<FGuid> Clients = AssignPropertyComboBox::GetDisplayedClients(ClientManager, DisplayedProperty, EditedObjects);
		
		const ConcertSharedSlate::FGetClientParenthesesContent GetParenthesesContent =
			ConcertClientSharedSlate::MakeGetLocalClientParenthesesContent(LocalConcertClient);
		const auto SortPredicate = [&GetParenthesesContent](const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right)
		{
			return ConcertSharedSlate::SortLocalClientParenthesesFirstThenThenAlphabetical(Left, Right, GetParenthesesContent);
		};
		
		return SWidgetType::GetDisplayString(
			Clients,
			ConcertClientSharedSlate::MakeClientInfoGetter(LocalConcertClient),
			ConcertSharedSlate::FClientSortPredicate::CreateLambda(SortPredicate),
			GetParenthesesContent
			);
	}

	void SAssignPropertyComboBox::Construct(const FArguments& InArgs,
	    TSharedRef<ConcertSharedSlate::IMultiReplicationStreamEditor> InEditor,
	    TSharedRef<IConcertClient> InConcertClient,
	    FOnlineClientManager& InClientManager,
	    FAssignPropertyModel& InModel
	)
	{
		Editor = MoveTemp(InEditor);
		ConcertClient = MoveTemp(InConcertClient);
		ClientManager = &InClientManager;
		Model = &InModel;
		
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
				SAssignNew(ClientListWidget, ConcertSharedSlate::SHorizontalClientList)
				.GetClientParenthesesContent(ConcertClientSharedSlate::MakeGetLocalClientParenthesesContent(ConcertClient.ToSharedRef()))
				.GetClientInfo(ConcertClientSharedSlate::MakeClientInfoGetter(ConcertClient.ToSharedRef()))
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.HighlightText_Lambda([this](){ return HighlightText ? *HighlightText : FText::GetEmpty(); })
				.EmptyListSlot() [ SNew(SNoClients) ]
			]
			.OnGetMenuContent(this, &SAssignPropertyComboBox::GetMenuContent)
		];

		Model->OnOwnershipChanged().AddSP(this, &SAssignPropertyComboBox::RebuildSubscriptionsAndRefresh);
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
		using namespace ConcertSharedSlate;

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
		Model->TogglePropertyFor(EndpointId, EditedObjects, Property);
		OnOptionClickedDelegate.ExecuteIfBound();
	}

	bool SAssignPropertyComboBox::CanClickOptionWithReason(const FGuid& EndpointId, FText* Reason) const
	{
		return Model->CanChangePropertyFor(EndpointId, Reason);
	}
	
	ECheckBoxState SAssignPropertyComboBox::GetOptionCheckState(const FGuid EndpointId) const
	{
		switch (Model->GetPropertyOwnershipState(EndpointId,EditedObjects, Property))
		{
		case EPropertyOnObjectsOwnershipState::OwnedOnAllObjects: return ECheckBoxState::Checked;
		case EPropertyOnObjectsOwnershipState::NotOwnedOnAllObjects: return ECheckBoxState::Unchecked;
		case EPropertyOnObjectsOwnershipState::Mixed: return ECheckBoxState::Undetermined;
		default: return ECheckBoxState::Undetermined;
		}
	}

	void SAssignPropertyComboBox::OnClickClear()
	{
		Model->ClearProperty(EditedObjects, Property);
		OnOptionClickedDelegate.ExecuteIfBound();
	}

	bool SAssignPropertyComboBox::CanClickClear() const
	{
		return Model->CanClear(EditedObjects, Property);
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