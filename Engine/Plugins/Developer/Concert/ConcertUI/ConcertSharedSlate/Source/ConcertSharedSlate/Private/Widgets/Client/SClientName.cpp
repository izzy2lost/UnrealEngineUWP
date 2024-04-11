// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Client/SClientName.h"

#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
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
			SNew(SHorizontalBox)
			
			// The user "Avatar color" displayed as a small square colored by the user avatar color.
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SImage)
				.ColorAndOpacity(this, &SClientName::GetAvatarColor)
				.Image(FAppStyle::GetBrush("Icons.FilledCircle"))
			]
					
			// The user "Display Name".
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(1, 0, 0, 0)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("NoBorder"))
				.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f))
				[
					SNew(STextBlock)
					.Font(InArgs._Font)
					.Text(TAttribute<FText>::CreateSP(this, &SClientName::GetClientDisplayName))
					.HighlightText(InArgs._HighlightText)
				]
			]
		];
	}

	FText SClientName::GetDisplayText(const FConcertClientInfo& Info, bool bDisplayAsLocalClient)
	{
		if (bDisplayAsLocalClient)
		{
			return FText::Format(
				LOCTEXT("ClientDisplayNameFmt", "{0} (You)"),
				FText::FromString(Info.DisplayName)
				);
		}
		
		return FText::FromString(Info.DisplayName);
	}

	FText SClientName::GetClientDisplayName() const
	{
		const TOptional<FConcertClientInfo> ClientInfo = ClientInfoAttribute.Get();
		return ClientInfo
			? GetDisplayText(*ClientInfo, DisplayAsLocalClientAttribute.Get())
			: LOCTEXT("Unavailable", "Unavailable");
	}

	FSlateColor SClientName::GetAvatarColor() const
	{
		const TOptional<FConcertClientInfo> ClientInfo = ClientInfoAttribute.Get();
		return ClientInfo ? ClientInfo->AvatarColor : FSlateColor(FLinearColor::Gray);
	}
}

#undef LOCTEXT_NAMESPACE