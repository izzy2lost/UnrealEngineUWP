// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSingleClientToolbar.h"

#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"

namespace UE::MultiUserClient
{
	void SSingleClientToolbar::Construct(const FArguments& InArgs)
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
		];
	}
}