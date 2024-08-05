// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Property/IPropertySourceProcessor.h"

#include "HAL//Platform.h"
#include "Templates/UnrealTemplate.h"

namespace UE::ConcertSharedSlate { class IEditableReplicationStreamModel; }

namespace UE::MultiUserClient::Replication
{
	class FOnlineClientManager;
	
	/**
	 * Injected into UI causing it to only display the properties that
	 * - have been selected by the user
	 * - are referenced by any client streams
	 */
	class FUserPropertySelectionSource
		: public ConcertSharedSlate::IPropertySourceProcessor
		, public FNoncopyable
	{
	public:

		FUserPropertySelectionSource(
			const ConcertSharedSlate::IEditableReplicationStreamModel& InUserSelection UE_LIFETIMEBOUND,
			const FOnlineClientManager& InClientManager UE_LIFETIMEBOUND
			);

		//~ Begin IPropertySourceProcessor Interface
		virtual void ProcessPropertySource(
			const ConcertSharedSlate::FPropertySourceContext& Context,
			TFunctionRef<void(const ConcertSharedSlate::IPropertySource& Model)> Processor
			) const override;
		//~ End IPropertySourceProcessor Interface

	private:

		/** Used to get the properties the user has selected. */
		const ConcertSharedSlate::IEditableReplicationStreamModel& UserSelection;
		/** Used to get client stream content and subscribe to changes. */
		const FOnlineClientManager& ClientManager;
	};
}
