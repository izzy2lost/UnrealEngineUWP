// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

namespace UE::MultiUserClient
{
	/** Customizes UMultiUserReplicationSettings to show only the settings relevant for joining */
	class FShowJoinSettingsCustomization : public IDetailCustomization
	{
	public:

		static TSharedRef<IDetailCustomization> Make();
		
		//~ Begin IDetailCustomization Interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
		//~ End IDetailCustomization Interface
	};
}
