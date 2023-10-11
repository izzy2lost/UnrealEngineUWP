// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IConcertClient;

namespace UE::ConcertClientSharedSlate
{
	/** Knows how to display FConcertClientInfo. */
	class CONCERTCLIENTSHAREDSLATE_API SClientName : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SClientName)
			: _DisplayAsLocalClient(false)
		{}
			/** The client info to display. */
			SLATE_ATTRIBUTE(const FConcertClientInfo*, ClientInfo)
			/** Whether visually indicate that this is a local client (appends "(me)" if true). */
			SLATE_ATTRIBUTE(bool, DisplayAsLocalClient)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	private:

		/** The client info to display. */
		TAttribute<const FConcertClientInfo*> ClientInfoAttribute;
		/** Whether visually indicate that this is a local client (appends "(me)" if true). */
		TAttribute<bool> DisplayAsLocalClientAttribute;

		/** Gets the display name. */
		FText GetClientDisplayName() const;
	};
}
