// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Async.h"
#include "DetailsViewArgs.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubUEClientInfo.h"
#include "LiveLinkHubProvider.h"
#include "LiveLinkTypes.h"
#include "PropertyEditorModule.h"
#include "SLiveLinkHubClientsView.h"
#include "UObject/StructOnScope.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"


/** Controller responsible for holding the list of connected clients and creating the clients view. */
class FLiveLinkHubClientsController
{
public:
	FLiveLinkHubClientsController(const TSharedRef<ILiveLinkHubClientsModel>& InClientsModel)
		: ClientsModel(InClientsModel)
	{
		ClientsModel->OnClientEvent().AddRaw(this, &FLiveLinkHubClientsController::OnClientEvent);
		
		UEClientInfo = MakeShared<TStructOnScope<FLiveLinkHubUEClientInfo>>();
	}

	~FLiveLinkHubClientsController()
	{
		ClientsModel->OnClientEvent().RemoveAll(this);
	}

	/** Create the widget that displays connected UE clients. */
	TSharedRef<SWidget> MakeClientsView()
	{
		return SAssignNew(ClientsView, SLiveLinkHubClientsView, ClientsModel.ToSharedRef())
			.OnClientSelected_Raw(this, &FLiveLinkHubClientsController::OnClientSelected);
	}

	/** Create the widget that displays information about a given UE client. */
	TSharedRef<SWidget> MakeClientDetailsView()
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = NAME_None;
		DetailsViewArgs.bShowOptions = false;

		FStructureDetailsViewArgs StructDetailsArgs;
		StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructDetailsArgs, UEClientInfo);
		StructDetailsView->GetDetailsView()->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([](const FPropertyAndParent&){ return true ;}));
		
		return StructDetailsView->GetWidget().ToSharedRef();
	}

private:
	/** Update the client info details if we received new info about it. */
	void OnClientEvent(FMessageAddress MessageAddress, ILiveLinkHubClientsModel::EClientEventType EventType)
	{
		switch (EventType)
		{
			case ILiveLinkHubClientsModel::EClientEventType::Connected:
				break;
			case ILiveLinkHubClientsModel::EClientEventType::Disconnected:
				break;
			case ILiveLinkHubClientsModel::EClientEventType::Modified:
			{
				if (FMessageAddress ClientAddress = ClientsView->GetSelectedClient(); ClientAddress.IsValid())
				{
					if (MessageAddress == ClientAddress)
					{
						UpdateClientDetails(MessageAddress);
					}
				}
				break;
			}
			default:
			{
				checkNoEntry();
			}
		}
	}

	/** Handler called when a client is selected in the clients view. */
	void OnClientSelected(FMessageAddress Client)
	{
		if (Client.IsValid())
		{
			UpdateClientDetails(Client);
		}
		else
		{
			// Disable this for the time being, this hides the details panel when you click on the empty list
			//StructDetailsView->SetStructureData(nullptr);
		}
	}
	
	/** Handles updating client details in the client details panel. */
	void UpdateClientDetails(FMessageAddress Client)
	{
		if (TOptional<FLiveLinkHubUEClientInfo> ClientInfo = ClientsModel->GetClientInfo(Client))
		{
			UEClientInfo->InitializeAs<FLiveLinkHubUEClientInfo>(*ClientInfo);
			StructDetailsView->SetStructureData(UEClientInfo);
		}
		
	}

private:
	/** List view of UE clients the hub can connect to. */
	TSharedPtr<SLiveLinkHubClientsView> ClientsView;
	/** Ptr to the livelink hub. */
	TSharedPtr<ILiveLinkHubClientsModel> ClientsModel;
	/** Holds a struct representing information about the currently selected client. */
	TSharedPtr<TStructOnScope<FLiveLinkHubUEClientInfo>> UEClientInfo;
	/** Holds the details view created with the property editor module. */
	TSharedPtr<IStructureDetailsView> StructDetailsView;
	/** Delegate called when one of the connection's info has changed. */
	FDelegateHandle ConnectionStatusChangedHandle;
};
