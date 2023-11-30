// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if SOURCE_CONTROL_WITH_SLATE

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

		SLATE_ATTRIBUTE(bool, IsEnabledSyncLatest)
		SLATE_ATTRIBUTE(bool, IsEnabledCheckInChanges)
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
	bool IsSourceControlSyncEnabled() const;
	bool HasSourceControlChangesToSync() const;
	EVisibility GetSourceControlSyncStatusVisibility() const;
	FText GetSourceControlSyncStatusText() const;
	FText GetSourceControlSyncStatusTooltipText() const;
	const FSlateBrush* GetSourceControlSyncStatusIcon() const;
	FReply OnSourceControlSyncClicked() const;

	/** Check-in button */
	int GetNumLocalChanges() const;
	bool IsSourceControlCheckInEnabled() const;
	bool HasSourceControlChangesToCheckIn() const;
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

	TAttribute<bool> IsSyncLatestEnabled;
	TAttribute<bool> IsCheckInChangesEnabled;

	/** Is there a conflict remaining? */
	bool bConflictsRemaining;

	FDelegateHandle SourceControlProviderChangedHandle;
	FDelegateHandle SourceControlStateChangedHandle;
};

#endif // SOURCE_CONTROL_WITH_SLATE