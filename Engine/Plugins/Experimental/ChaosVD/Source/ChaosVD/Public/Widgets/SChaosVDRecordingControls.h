// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StatusBarSubsystem.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

#include "SChaosVDRecordingControls.generated.h"

namespace Chaos::VisualDebugger
{
	struct FChaosVDOptionalDataChannel;
}

class SButton;
class SChaosVDMainTab;
class FReply;
struct FSlateBrush;

UCLASS()
class CHAOSVD_API UChaosVDRecordingToolbarMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<class SChaosVDRecordingControls> RecordingControlsWidget;
};

UENUM()
enum class EChaosVDRecordingMode
{
	File,
	Live
};

typedef Chaos::VisualDebugger::FChaosVDOptionalDataChannel FCVDDataChannel;

class SChaosVDRecordingControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SChaosVDRecordingControls ){}
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, const TSharedRef<SChaosVDMainTab>& InMainTabSharedRef);

	virtual ~SChaosVDRecordingControls() override;

protected:

	TSharedRef<SWidget> GenerateToggleRecordingStateButton(EChaosVDRecordingMode RecordingMode, const FText& StartRecordingTooltip);
	TSharedRef<SWidget> GenerateDataChannelsMenu();
	TSharedRef<SWidget> GenerateDataChannelsButton();
	TSharedRef<SWidget> GenerateRecordingTimeTextBlock();
	TSharedRef<SWidget> GenerateToolbarWidget();

#if WITH_CHAOS_VISUAL_DEBUGGER
	void ToggleChannelEnabledState(TWeakPtr<FCVDDataChannel> Channel);
	bool IsChannelEnabled(TWeakPtr<FCVDDataChannel> Channel);
	bool CanChangeChannelEnabledState(TWeakPtr<FCVDDataChannel> Channel);
#endif

	bool HasDataChannelsSupport() const;
	
	const FSlateBrush* GetRecordOrStopButton(EChaosVDRecordingMode RecordingMode) const;
	
	void HandleRecordingStop();
	void HandleRecordingStart();
	void AttemptToConnectToLiveSession();

	FReply ToggleRecordingState(EChaosVDRecordingMode RecordingMode);

	bool IsRecordingToggleButtonEnabled(EChaosVDRecordingMode RecordingMode) const;
	EVisibility IsRecordingToggleButtonVisible(EChaosVDRecordingMode RecordingMode) const;

	void RegisterMenus();

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

	bool bRecordingButtonHovered = false;

	FCurveSequence RecordingAnimation;

	static inline const FName RecordingControlsToolbarName = FName("ChaosVD.MainToolBar.RecordingControls");
};
