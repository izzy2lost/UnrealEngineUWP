// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosBrowseTraceFileSourceModal.h"

#include "Editor.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosBrowseTraceFileSourceModal::Construct(const FArguments& InArgs)
{
	SWindow::Construct(SWindow::FArguments()
	.Title(LOCTEXT("SChaosVDBrowseFileModal_Title", "CVD Recording Source Selector"))
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	.UserResizeBorder(0)
	.ClientSize(FVector2D(350, 80))
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(15)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectSourceMessage", "Where is the recording file located?"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("BrowseFolder", "Folder"))
					.OnClicked(this, &SChaosBrowseTraceFileSourceModal::OnButtonClick, EChaosVDBrowseFileModalResponse::OpenFolder)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("BrowseTraceStore", "Trace Store"))
					.OnClicked(this, &SChaosBrowseTraceFileSourceModal::OnButtonClick, EChaosVDBrowseFileModalResponse::OpenTraceStore)
				]
			]
	]);
}

EChaosVDBrowseFileModalResponse SChaosBrowseTraceFileSourceModal::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

FReply SChaosBrowseTraceFileSourceModal::OnButtonClick(EChaosVDBrowseFileModalResponse Response)
{
	UserResponse = Response;
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

