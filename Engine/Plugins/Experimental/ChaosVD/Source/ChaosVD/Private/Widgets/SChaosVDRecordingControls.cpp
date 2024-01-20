// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDRecordingControls.h"

#include "ChaosVDEngine.h"
#include "ChaosVDStyle.h"
#include "ChaosVDRuntimeModule.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Editor.h"
#include "Input/Reply.h"
#include "Misc/MessageDialog.h"
#include "StatusBarSubsystem.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SChaosVDMainTab.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDRecordingControls::Construct(const FArguments& InArgs, const TWeakPtr<SChaosVDMainTab>& InMainTabWeakPtr)
{
	if (TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = InMainTabWeakPtr.Pin())
	{
		MainTabWeakPtr = MainTabSharedPtr;
		StatusBarID = MainTabSharedPtr->GetStatusBarName();
	}
	else
	{
		ensureMsgf(false, TEXT("Constructed with an invalid Main Tab"));
	}
	
	ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.Padding(FMargin(12, 7, 2, 7))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
					.OnClicked(FOnClicked::CreateRaw(this, &SChaosVDRecordingControls::ToggleRecordingState))
					.ForegroundColor(FSlateColor::UseForeground() )
					.IsFocusable(false)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew( SImage )
						.ToolTipText_Lambda([this]()
						{
							return IsRecording() ? LOCTEXT("StopRecordButtonDesc", "Stop the current recording ")
													: LOCTEXT("RecordButtonDesc", "Starts a recording for the current sessions");
						})
						.Image_Raw(this, &SChaosVDRecordingControls::GetRecordOrStopButton)
						.ColorAndOpacity(FColor::Red)
					]
					]

			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(FMargin(12, 0, 0, 0))
			[
				SNew( STextBlock )
					.TextStyle(FAppStyle::Get(), "SmallButtonText")
					.Text_Raw(this, &SChaosVDRecordingControls::GetRecordingTimeText)
					.ColorAndOpacity(FColor::White)
			]
		]
	];

	RecordingStartedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStartedCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &SChaosVDRecordingControls::HandleRecordingStart));
	RecordingStoppedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &SChaosVDRecordingControls::HandleRecordingStop));
}

SChaosVDRecordingControls::~SChaosVDRecordingControls()
{
	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStartedCallback(RecordingStartedHandle);
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(RecordingStoppedHandle);
	}
}

const FSlateBrush* SChaosVDRecordingControls::GetRecordOrStopButton() const
{
	return IsRecording() ? FChaosVDStyle::Get().GetBrush("StopIcon") : FChaosVDStyle::Get().GetBrush("RecordIcon");
}

void SChaosVDRecordingControls::HandleRecordingStop()
{
	const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin();
	if (!MainTabSharedPtr.IsValid())
	{
		return;
	}

	const bool bIsLiveSession = MainTabSharedPtr->GetChaosVDEngineInstance()->GetCurrentSessionDescriptor().bIsLiveSession;

	if (UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr)
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, RecordingMessageHandle);

		if (bIsLiveSession)
		{
			const FText LiveSessionEnded = LOCTEXT("LiveSessionEndedMessage"," Live session has ended");
			LiveSessionEndedMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, LiveSessionEnded);
		}
		else
		{
			const FText RecordingPathMessage = FText::Format(LOCTEXT("RecordingSavedPathMessage"," Recoring saved at {0} "), FText::AsCultureInvariant(FChaosVDRuntimeModule::Get().GetActiveRecordingFileName()));
			RecordingPathMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, RecordingPathMessage);
		}
	}
	
	if (!bIsLiveSession)
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("OpenLastRecordingMessage", "Do you want to load the recorded file now? ")) == EAppReturnType::Yes)
		{
			MainTabSharedPtr->GetChaosVDEngineInstance()->LoadRecording(FChaosVDRuntimeModule::Get().GetActiveRecordingFileName());
		}
	}
}

void SChaosVDRecordingControls::HandleRecordingStart()
{
	UStatusBarSubsystem* StatusBarSubsystem = GEditor ? GEditor->GetEditorSubsystem<UStatusBarSubsystem>() : nullptr;
	if (!StatusBarSubsystem)
	{
		return;
	}
	
	if (RecordingPathMessageHandle.IsValid())
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, RecordingPathMessageHandle);
		RecordingPathMessageHandle = FStatusBarMessageHandle();
	}
	
	if (LiveSessionEndedMessageHandle.IsValid())
	{
		StatusBarSubsystem->PopStatusBarMessage(StatusBarID, LiveSessionEndedMessageHandle);
		LiveSessionEndedMessageHandle = FStatusBarMessageHandle();
	}

	RecordingMessageHandle = StatusBarSubsystem->PushStatusBarMessage(StatusBarID, LOCTEXT("RecordingMessgae", "Recording..."));
}

FReply SChaosVDRecordingControls::ToggleRecordingState()
{
	if (!IsRecording())
	{
		FChaosVDRuntimeModule::Get().StartRecording({});
	}
	else
	{
		FChaosVDRuntimeModule::Get().StopRecording();
	}

	return FReply::Handled();
}

bool SChaosVDRecordingControls::IsRecording() const
{
#if WITH_CHAOS_VISUAL_DEBUGGER && UE_TRACE_ENABLED
	return FChaosVisualDebuggerTrace::IsTracing();
#else
	return false;
#endif
}

FText SChaosVDRecordingControls::GetRecordingTimeText() const
{
	FNumberFormattingOptions FormatOptions;
	FormatOptions.MinimumFractionalDigits = 2;
	FormatOptions.MaximumFractionalDigits = 2;
	FText SecondsText = FText::AsNumber(FChaosVDRuntimeModule::Get().GetAccumulatedRecordingTime(), &FormatOptions);
	
	return FText::Format(LOCTEXT("RecordingTimer","{0} s"), SecondsText);
}

#undef LOCTEXT_NAMESPACE 
