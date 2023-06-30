// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatusBarSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class SChaosVDMainTab;
class FReply;
struct FSlateBrush;

class SChaosVDRecordingControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SChaosVDRecordingControls ){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TWeakPtr<SChaosVDMainTab>& InMainTabWeakPtr);

	virtual ~SChaosVDRecordingControls() override;

protected:
	
	const FSlateBrush* GetRecordOrStopButton() const;
	
	void HandleRecordingStop();
	void HandleRecordingStart();

	FReply ToggleRecordingState();

	bool IsRecording() const;

	FText GetRecordingTimeText() const;
	
	FName StatusBarID;
	
	FStatusBarMessageHandle RecordingMessageHandle;
	FStatusBarMessageHandle RecordingPathMessageHandle;
	FDelegateHandle RecordingStartedHandle;
	FDelegateHandle RecordingStoppedHandle;

	TWeakPtr<SChaosVDMainTab> MainTabWeakPtr;
};
