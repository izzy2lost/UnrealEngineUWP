// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	/**
	 * Displays the name of a client.
	 * 
	 * The name will look like "Client Name".
	 * @see SLocalClientName
	 *
	 * If the client disconnects, the last known info is used.
	 * If the client info is unknown, the widget will display an empty FConcertClientInfo;
	 */
	class CONCERTCLIENTSHAREDSLATE_API SRemoteClientName : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SRemoteClientName)
		{}
			SLATE_ATTRIBUTE(FGuid, ClientEndpointId)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient);

	private:

		/** The displayed local client. */
		TSharedPtr<IConcertClient> Client;
		/** The endpoint ID of the client to display. */
		TAttribute<FGuid> ClientEndpointIdAttribute;

		/**
		 * Cached so that the info remains known when the client disconnects.
		 * Must be mutable because TAttribute::CreateSP requires GetClientInfo to be const.
		 */
		mutable FConcertSessionClientInfo LastKnownClientInfo;

		/** Gets the display info. */
		const FConcertClientInfo* GetClientInfo() const;
	};
}
