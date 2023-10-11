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
	 * The name will look like "Client Name (me)".
	 * @see SRemoteClientName
	 */
	class CONCERTCLIENTSHAREDSLATE_API SLocalClientName : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SLocalClientName)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, TSharedRef<IConcertClient> InClient);
	};
}
