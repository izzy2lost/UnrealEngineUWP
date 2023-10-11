// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;
class SWidgetSwitcher;

namespace UE::MultiUserClient
{
	class FRemoteReplicationClient;
	
	/**
	 * Wraps a SComboButton that is used to select which SReplicationClientView should be displayed in a SWidgetSwitcher.
	 */
	class SSelectClientViewComboButton : public SCompoundWidget
	{
	public:

		DECLARE_DELEGATE_OneParam(FSelectClient, const FGuid&);
		
		SLATE_BEGIN_ARGS(SSelectClientViewComboButton)
		{}
			/** Uses to query local and remote client display info. */
			SLATE_ARGUMENT(TSharedPtr<IConcertClient>, Client)
		
			/** Remote clients that can be selected from in the order that they should be displayed. */
			SLATE_ATTRIBUTE(TArray<FGuid>, SelectableClients)
			/** The client that is currently selected */
			SLATE_ATTRIBUTE(FGuid, CurrentSelection)

			/** Called when a client is selected */
			SLATE_EVENT(FSelectClient, OnSelectClient)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		/** Uses to query local and remote client display info. */
		TSharedPtr<IConcertClient> Client;
		
		/** Remote clients that can be selected from */
		TAttribute<TArray<FGuid>> ClientsAttribute;
		/** The current client selection */
		TAttribute<FGuid> CurrentSelection;
		
		/** Called when a client is selected */
		FSelectClient OnSelectClientDelegate;

		enum class EButtonContent 
		{
			LocalClient = 0,
			RemoteClient = 1
		};
		/** Content for the button */
		TSharedPtr<SWidgetSwitcher> ButtonContent;
		
		TSharedRef<SWidget> MakeMenuContent();
		
		int32 GetActiveWidgetIndex() const;
		FGuid GetSelectedClientEndpointId() const;
	};
}

