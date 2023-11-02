// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSingleClientToolbar.h"

#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"

#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

namespace UE::MultiUserClient
{
	void SSingleClientToolbar::Construct(const FArguments& InArgs, const ConcertClientSharedSlate::IObjectToPropertiesModel& InObjectModel, FGlobalAuthorityCache& InAuthorityCache)
	{
		ChildSlot
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(5.f, 0.f)
			[
				InArgs._ViewSelectionArea.Widget
			]
			
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 5.f, 0.f)
			[
				SNew(SReplicationStatus, InObjectModel, InAuthorityCache)
				.DisplayedClients(InArgs._DisplayedClients)
			]
		];
	}
}