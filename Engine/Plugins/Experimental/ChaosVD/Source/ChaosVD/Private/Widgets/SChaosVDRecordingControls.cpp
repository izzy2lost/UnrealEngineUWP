// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDRecordingControls.h"

#include "ChaosVDEngine.h"
#include "ChaosVDModule.h"
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

void SChaosVDRecordingControls::Construct(const FArguments& InArgs, const TSharedRef<SChaosVDMainTab>& InMainTabSharedRef)
{
	MainTabWeakPtr = InMainTabSharedRef;
	StatusBarID = InMainTabSharedRef->GetStatusBarName();

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
				GenerateToggleRecordingStateButton(EChaosVDRecordingMode::File, LOCTEXT("RecordButtonDesc", "Starts a recording for the current session, saving it directly to file"))
			]
			+SHorizontalBox::Slot()
			.Padding(5.0f,  0.0f, 0.0f, 0.0f)
			.HAlign(HAlign_Left)
			[
				GenerateToggleRecordingStateButton(EChaosVDRecordingMode::Live, LOCTEXT("RecordButtonDesc", "Starts a recording and automatically connects to it playing it back in real time"))
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(12.0f, 0.0f, 0.0f, 0.0f)
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

TSharedRef<SButton> SChaosVDRecordingControls::GenerateToggleRecordingStateButton(EChaosVDRecordingMode RecordingMode, const FText& StartRecordingTooltip)
{
	return SNew(SButton)
		.OnClicked(FOnClicked::CreateRaw(this, &SChaosVDRecordingControls::ToggleRecordingState, RecordingMode))
		.ForegroundColor(FSlateColor::UseForeground())
		.IsFocusable(false)
		.IsEnabled_Raw(this, &SChaosVDRecordingControls::IsRecordingToggleButtonEnabled, RecordingMode)
		.Visibility_Raw(this, &SChaosVDRecordingControls::IsRecordingToggleButtonVisible, RecordingMode)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.ToolTipText_Lambda([this, StartRecordingTooltip]()
		{
			return IsRecording() ? LOCTEXT("StopRecordButtonDesc", "Stop the current recording ") : StartRecordingTooltip;
		})
		[
			SNew(SImage)
			.Image_Raw(this, &SChaosVDRecordingControls::GetRecordOrStopButton, RecordingMode)
			.ColorAndOpacity_Lambda([this](){ return IsRecording() ? FColor::Red : FColor::White; })
		];
}


SChaosVDRecordingControls::~SChaosVDRecordingControls()
{
	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStartedCallback(RecordingStartedHandle);
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(RecordingStoppedHandle);
	}
}

const FSlateBrush* SChaosVDRecordingControls::GetRecordOrStopButton(EChaosVDRecordingMode RecordingMode) const
{
	const FSlateBrush* RecordIconBrush = RecordingMode == EChaosVDRecordingMode::File ? FChaosVDStyle::Get().GetBrush("RecordToFileIcon") : FChaosVDStyle::Get().GetBrush("RecordToLiveIcon");
	return IsRecording() ? FChaosVDStyle::Get().GetBrush("StopIcon") : RecordIconBrush;
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

void SChaosVDRecordingControls::AttemptToConnectToLiveSession()
{
	bAutoConnectionAttemptInProgress = true;
	// We need to wait at least one tick before attempting to connect
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis = AsWeak()](float DeltaTime)
	{
		if (const TSharedPtr<SChaosVDRecordingControls> RecordingControlsPtr = StaticCastSharedPtr<SChaosVDRecordingControls>(WeakThis.Pin()))
		{
			if (const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = RecordingControlsPtr->MainTabWeakPtr.Pin())
			{
				static FString SessionAddress(TEXT("127.0.0.1"));
			
				int32 SessionID = 0;

				FChaosVDTraceManager::EnumerateActiveSessions(SessionAddress, [&SessionID](const UE::Trace::FStoreClient::FSessionInfo& InSessionInfo)
				{
					SessionID = InSessionInfo.GetTraceId();

					// CVD stops all active sessions before staring a recording, so we know our session ID will be first;
					return false;
				});

				// CVD needs the trace session name to be able to load a live session. Although the session exist, the session name might not be written right away
				// Trace files don't really have metadata, it is all part of the same stream, so we need to wait until it is written which might take a few ticks.
				// Therefore if it is not ready, try again a few times.
				if (!MainTabSharedPtr->ConnectToLiveSession(SessionID, SessionAddress))
				{
					if (RecordingControlsPtr->CurrentConnectionAttempts <= RecordingControlsPtr->MaxAutoplayConnectionAttempts)
					{
						UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%s] Failed to connect to live session | Attempting again in [%f]..."), ANSI_TO_TCHAR(__FUNCTION__), RecordingControlsPtr->IntervalBetweenAutoplayConnectionAttemptsSeconds);
						RecordingControlsPtr->AttemptToConnectToLiveSession();
					}
					else
					{
						RecordingControlsPtr->bAutoConnectionAttemptInProgress = false;
						UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to live session | [%d] attempts exhausted..."), ANSI_TO_TCHAR(__FUNCTION__), RecordingControlsPtr->MaxAutoplayConnectionAttempts);	
					}
				}
				else
				{
					RecordingControlsPtr->bAutoConnectionAttemptInProgress = false;
				}
			}
		}
		return false;
	}), IntervalBetweenAutoplayConnectionAttemptsSeconds);
}

FReply SChaosVDRecordingControls::ToggleRecordingState(EChaosVDRecordingMode RecordingMode)
{
	if (!IsRecording())
	{
		TArray<FString, TInlineAllocator<1>> RecordingArgs;

		if (RecordingMode == EChaosVDRecordingMode::Live)
		{
			RecordingArgs.Emplace(TEXT("Server"));

			FChaosVDRuntimeModule::Get().StartRecording(RecordingArgs);

			AttemptToConnectToLiveSession();
		}
		else
		{
			FChaosVDRuntimeModule::Get().StartRecording(RecordingArgs);
		}
	}
	else
	{
		FChaosVDRuntimeModule::Get().StopRecording();
	}

	return FReply::Handled();
}

bool SChaosVDRecordingControls::IsRecordingToggleButtonEnabled(EChaosVDRecordingMode RecordingMode) const
{
	if (bAutoConnectionAttemptInProgress)
	{
		return false;
	}

	const TSharedPtr<SChaosVDMainTab> MainTabSharedPtr = MainTabWeakPtr.Pin();
	if (!MainTabSharedPtr.IsValid())
	{
		return false;

	}
	const bool bIsLiveSession = MainTabSharedPtr->GetChaosVDEngineInstance()->GetCurrentSessionDescriptor().bIsLiveSession;

	const bool bIsRecording = IsRecording();

	if (RecordingMode == EChaosVDRecordingMode::File)
	{
		return (bIsRecording && !bIsLiveSession) || !bIsRecording;
	}
	else if (RecordingMode == EChaosVDRecordingMode::Live && GEditor)
	{
		return (bIsRecording && bIsLiveSession) || (!bIsRecording && GEditor->IsPlayingSessionInEditor());
	}

	return false;
}

EVisibility SChaosVDRecordingControls::IsRecordingToggleButtonVisible(EChaosVDRecordingMode RecordingMode) const
{
	// If we are recording, don't show the stop button for the mode that is disabled
	const bool bIsRecording = IsRecording();
	const bool bShouldButtonBeVisible = bIsRecording ? bIsRecording && IsRecordingToggleButtonEnabled(RecordingMode) : true;
	return bShouldButtonBeVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SChaosVDRecordingControls::IsRecording() const
{
#if WITH_CHAOS_VISUAL_DEBUGGER
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
