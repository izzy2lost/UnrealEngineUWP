// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatusBarSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SChaosVDMainTab;
class FReply;
struct FSlateBrush;

UENUM()
enum class EChaosVDRecordingMode
{
	File,
	Live
};

class SChaosVDRecordingControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SChaosVDRecordingControls ){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedRef<SChaosVDMainTab>& InMainTabSharedRef);

	virtual ~SChaosVDRecordingControls() override;

protected:

	TSharedRef<SButton>GenerateToggleRecordingStateButton(EChaosVDRecordingMode RecordingMode, const FText& StartRecordingTooltip);

	const FSlateBrush* GetRecordOrStopButton(EChaosVDRecordingMode RecordingMode) const;
	
	void HandleRecordingStop();
	void HandleRecordingStart();
	void AttemptToConnectToLiveSession();

	FReply ToggleRecordingState(EChaosVDRecordingMode RecordingMode);

	bool IsRecordingToggleButtonEnabled(EChaosVDRecordingMode RecordingMode) const;
	EVisibility IsRecordingToggleButtonVisible(EChaosVDRecordingMode RecordingMode) const;

	bool IsRecording() const;

	FText GetRecordingTimeText() const;
	
	FName StatusBarID;
	
	FStatusBarMessageHandle RecordingMessageHandle;
	FStatusBarMessageHandle RecordingPathMessageHandle;
	FStatusBarMessageHandle LiveSessionEndedMessageHandle;
	FDelegateHandle RecordingStartedHandle;
	FDelegateHandle RecordingStoppedHandle;

	TWeakPtr<SChaosVDMainTab> MainTabWeakPtr;

	int32 MaxAutoplayConnectionAttempts = 20;
	float IntervalBetweenAutoplayConnectionAttemptsSeconds = 0.1f;
	bool bAutoConnectionAttemptInProgress = false;
	int32 CurrentConnectionAttempts = 0;
};
