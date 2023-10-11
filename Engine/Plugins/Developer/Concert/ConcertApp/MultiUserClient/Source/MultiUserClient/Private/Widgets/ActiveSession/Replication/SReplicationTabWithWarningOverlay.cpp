// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationTabWithWarningOverlay.h"

#include "SReplicationRootWidget.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicationIsExperimentalWarning"

namespace UE::MultiUserClient
{
	void SReplicationTabWithWarningOverlay::Construct(
		const FArguments& InArgs,
		TSharedRef<FMultiUserReplicationManager> InReplicationManager,
		TSharedRef<IConcertSyncClient> InClient)
	{
		ChildSlot
		[
			SAssignNew(Overlay, SOverlay)

			// The real content
			+SOverlay::Slot()
			[
				SNew(SReplicationRootWidget, InReplicationManager, InClient)
			]

			//  
			+SOverlay::Slot()
			.ZOrder(1)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.BorderBackgroundColor(FSlateColor(FLinearColor(0.6, 0.6, 0.6, 0.8f)))
				.Padding(0)
				[
					SNew(SBox)
					.Padding(0.f, 0.f, 0.f, 10.f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Clipping(EWidgetClipping::ClipToBounds)
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EnableLoggingVisibility", "Warning: Replication is a work in progress feature."))
							.Justification(ETextJustify::Center)
						]

						+SVerticalBox::Slot()
						.Padding(0, 4, 0, 0)
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(SButton)
							.OnClicked_Lambda([this]()
							{
								Overlay->RemoveSlot(1);
								return FReply::Handled();
							})
							[
								SNew(STextBlock)
								.Text(LOCTEXT("EnableLoggingVisibility.Button.Text", "Ignore"))
							]
						]
					]
				]
			]
		];
	}
}

#undef LOCTEXT_NAMESPACE