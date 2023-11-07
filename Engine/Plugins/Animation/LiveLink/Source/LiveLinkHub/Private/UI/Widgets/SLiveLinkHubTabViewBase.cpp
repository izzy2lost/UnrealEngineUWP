// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkHubTabViewBase.h"

#include "LiveLinkHub.h"
#include "LiveLinkHubModule.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Recording/LiveLinkHubRecordingController.h"
#include "SLiveLinkHubStatusBar.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/STimecode.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub"

TSharedRef<SWidget> GetModeSwitcherContent()
{
	FMenuBuilder MenuBuilder(true, NULL);
	{
		FUIAction CreatorModeAction(FExecuteAction::CreateLambda([](){}));
		MenuBuilder.AddMenuEntry(LOCTEXT("CreatorModeLabel", "Creator Mode"), LOCTEXT("CreatorModeHint", "-Placeholder text-"), FSlateIcon(), CreatorModeAction);
		
		FUIAction StudioModeAction(FExecuteAction::CreateLambda([](){}));
		MenuBuilder.AddMenuEntry(LOCTEXT("StudioModeLabel", "Studio Mode"), LOCTEXT("StudioModeHint", "-Placeholder text-"), FSlateIcon(), StudioModeAction);
	}

	return MenuBuilder.MakeWidget();
}

void SLiveLinkHubTabViewBase::Construct(const FArguments& InArgs)
{
	TSharedPtr<FLiveLinkHub> LiveLinkHub = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkHub();
	const FName StatusBarId = TEXT("LiveLinkHubStatusBar");
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(1.0f, 2.0f))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(0.05f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2.f)
				.HAlign(HAlign_Left)
				[
					// Should be replaced by the mode switcher widget
					SNew(SComboButton)
					.ContentPadding(3.f)
					.OnGetMenuContent_Static(&::GetModeSwitcherContent)
					.ButtonContent()
					[
						// Should switch depending on the currently selected mode.
						SNew(STextBlock).Text(LOCTEXT("SelectModeLabl", "Creator Mode"))
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(2.f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						LiveLinkHub->GetRecordingController()->MakeRecordToolbarEntry()
					]
					+ SHorizontalBox::Slot()
					.Padding(2.f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SSeparator)
						.Orientation(Orient_Vertical)
					]
					+ SHorizontalBox::Slot()
					.Padding(2.f)
                    .VAlign(VAlign_Center)
					[
						SNew(STimecode)
						.Timecode(MakeAttributeLambda([]
						{
							return FApp::GetTimecode();
						}))
					]
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(1.0f, 2.0f)
			[
				InArgs._Content.Widget
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
				.Orientation(Orient_Horizontal)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SLiveLinkHubStatusBar, StatusBarId)
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE // LiveLinkHub
