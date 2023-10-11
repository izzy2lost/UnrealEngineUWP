// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ClientName/SClientName.h"

#include "ConcertFrontendUtils.h"

#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SClientName"

namespace UE::ConcertClientSharedSlate
{
	void SClientName::Construct(const FArguments& InArgs)
	{
		ClientInfoAttribute = InArgs._ClientInfo;
		DisplayAsLocalClientAttribute = InArgs._DisplayAsLocalClient;
		check(ClientInfoAttribute.IsSet() || ClientInfoAttribute.IsBound());
		check(DisplayAsLocalClientAttribute.IsSet() || DisplayAsLocalClientAttribute.IsBound());
		
		ChildSlot
		[
			ConcertFrontendUtils::CreateDisplayName(
				TAttribute<FText>::CreateSP(this, &SClientName::GetClientDisplayName)
				)
		];
	}

	FText SClientName::GetClientDisplayName() const
	{
		const FConcertClientInfo* ClientInfo = ClientInfoAttribute.Get();
		check(ClientInfo);
		
		if (DisplayAsLocalClientAttribute.Get())
		{
			return FText::Format(
				LOCTEXT("ClientDisplayNameFmt", "{0} (me)"),
				FText::FromString(ClientInfo->DisplayName)
				);
		}
		
		return FText::FromString(ClientInfo->DisplayName);
	}
}

#undef LOCTEXT_NAMESPACE