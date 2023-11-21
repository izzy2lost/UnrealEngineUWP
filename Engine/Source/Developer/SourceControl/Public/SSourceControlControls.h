// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/SlateDelegates.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

/** Widget for displaying Source Control Check in Changes and Sync Latest buttons */
class SOURCECONTROL_API SSourceControlControls : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSourceControlControls) {}

		SLATE_EVENT(FOnClicked, OnClickedSyncLatest)
		SLATE_EVENT(FOnClicked, OnClickedCheckInChanges)
		SLATE_EVENT(FOnGetContent, OnGenerateKebabMenu)

	SLATE_END_ARGS()

public:
	/** Construct this widget */
	void Construct(const FArguments& InArgs);

private:
	/** Sync button */
	bool IsAtLatestRevision() const;
	bool CanSourceControlSync() const;
	EVisibility GetSourceControlSyncStatusVisibility() const;
	FText GetSourceControlSyncStatusText() const;
	FText GetSourceControlSyncStatusTooltipText() const;
	const FSlateBrush* GetSourceControlSyncStatusIcon() const;
	FReply OnSourceControlSyncClicked() const;

	/** Check-in button */
	int GetNumLocalChanges() const;
	bool CanSourceControlCheckIn() const;
	EVisibility GetSourceControlCheckInStatusVisibility() const;
	FText GetSourceControlCheckInStatusText() const;
	FText GetSourceControlCheckInStatusTooltipText() const;
	const FSlateBrush* GetSourceControlCheckInStatusIcon() const;
	FReply OnSourceControlCheckInChangesClicked() const;

	/** Conflicts */
	void CheckSourceControlStatus();
	bool AreConflictsRemaining() const;

	void OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);
	void OnSourceControlStateChanged();
	
private:
	
	FOnClicked OnSyncLatestClicked;
	FOnClicked OnCheckInChangesClicked;

	/** Is there a conflict remaining? */
	bool bConflictsRemaining;

	FDelegateHandle SourceControlProviderChangedHandle;
	FDelegateHandle SourceControlStateChangedHandle;
};