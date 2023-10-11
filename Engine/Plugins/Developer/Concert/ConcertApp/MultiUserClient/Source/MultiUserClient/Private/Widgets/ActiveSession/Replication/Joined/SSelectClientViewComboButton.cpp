// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSelectClientViewComboButton.h"

#include "IConcertClient.h"
#include "Widgets/ClientName/SLocalClientName.h"
#include "Widgets/ClientName/SRemoteClientName.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SWidgetSwitcher.h"

namespace UE::MultiUserClient
{
	void SSelectClientViewComboButton::Construct(const FArguments& InArgs)
	{
		Client = InArgs._Client;
		ClientsAttribute = InArgs._SelectableClients;
		CurrentSelection = InArgs._CurrentSelection;
		OnSelectClientDelegate = InArgs._OnSelectClient;
		
		ChildSlot
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &SSelectClientViewComboButton::MakeMenuContent)
			.ButtonContent()
			[
				SAssignNew(ButtonContent, SWidgetSwitcher)
				.WidgetIndex(this, &SSelectClientViewComboButton::GetActiveWidgetIndex)

				+SWidgetSwitcher::Slot()
				[
					SNew(ConcertClientSharedSlate::SLocalClientName, Client.ToSharedRef())
				]
				+SWidgetSwitcher::Slot()
				[
					SNew(ConcertClientSharedSlate::SRemoteClientName, Client.ToSharedRef())
					.ClientEndpointId(this, &SSelectClientViewComboButton::GetSelectedClientEndpointId)
				]
			]
		];
	}

	TSharedRef<SWidget> SSelectClientViewComboButton::MakeMenuContent()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		for (const FGuid& ClientId : ClientsAttribute.Get())
		{
			const bool bIsLocalClient = ClientId == Client->GetCurrentSession()->GetSessionClientEndpointId();
			FUIAction UIAction(
				FExecuteAction::CreateLambda([this, ClientId](){ OnSelectClientDelegate.Execute(ClientId); }),
				FCanExecuteAction::CreateLambda([this, ClientId](){ return CurrentSelection.Get() != ClientId; })
				);
			
			if (bIsLocalClient)
			{
				MenuBuilder.AddMenuEntry(UIAction, SNew(ConcertClientSharedSlate::SLocalClientName, Client.ToSharedRef()));
			}
			else
			{
				MenuBuilder.AddMenuEntry(UIAction, SNew(ConcertClientSharedSlate::SRemoteClientName, Client.ToSharedRef()).ClientEndpointId(ClientId));
			}
		}
		
		return MenuBuilder.MakeWidget();
	}

	int32 SSelectClientViewComboButton::GetActiveWidgetIndex() const
	{
		return CurrentSelection.Get() == Client->GetCurrentSession()->GetSessionClientEndpointId()
			? static_cast<int32>(EButtonContent::LocalClient)
			: static_cast<uint32>(EButtonContent::RemoteClient);
	}

	FGuid SSelectClientViewComboButton::GetSelectedClientEndpointId() const
	{
		return CurrentSelection.Get();
	}
}
