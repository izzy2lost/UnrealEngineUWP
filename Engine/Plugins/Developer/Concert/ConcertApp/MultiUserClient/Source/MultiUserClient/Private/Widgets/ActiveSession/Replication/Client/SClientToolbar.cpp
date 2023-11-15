// Copyright Epic Games, Inc. All Rights Reserved.

#include "SClientToolbar.h"

#include "Widgets/ActiveSession/Replication/Client/SReplicationStatus.h"

#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

namespace UE::MultiUserClient
{
	void SClientToolbar::Construct(const FArguments& InArgs, FGlobalAuthorityCache& InAuthorityCache)
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
				SAssignNew(ReplicationStatus, SReplicationStatus, InAuthorityCache)
				.DisplayedClients(InArgs._DisplayedClients)
				.ForEachReplicatedObject(InArgs._ForEachReplicatedObject)
			]
		];
	}

	void SClientToolbar::RefreshStatusText()
	{
		ReplicationStatus->RefreshStatusText();
	}
}
