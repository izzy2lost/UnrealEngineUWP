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
		SLATE_ATTRIBUTE(bool, IsEnabledSyncLatestSeparator)
		SLATE_ATTRIBUTE(bool, IsEnabledCheckInChangesSeparator)
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
	EVisibility GetSourceControlSyncSeparatorVisibility() const;
	FText GetSourceControlSyncStatusText() const;
	FText GetSourceControlSyncStatusTooltipText() const;
	const FSlateBrush* GetSourceControlSyncStatusIcon() const;
	FReply OnSourceControlSyncClicked() const;

	/** Check-in button */
	int GetNumLocalChanges() const;
	bool IsSourceControlCheckInEnabled() const;
	bool HasSourceControlChangesToCheckIn() const;
	EVisibility GetSourceControlCheckInStatusVisibility() const;
	EVisibility GetSourceControlCheckInSeparatorVisibility() const;
	FText GetSourceControlCheckInStatusText() const;
	FText GetSourceControlCheckInStatusTooltipText() const;
	const FSlateBrush* GetSourceControlCheckInStatusIcon() const;
	FReply OnSourceControlCheckInChangesClicked() const;

	/** Conflicts */
	void CheckSourceControlStatus();
public:
	bool AreConflictsRemaining() const;
	int32 GetNumConflictsRemaining() const;
	
	static void SetCheckInChangesDisabledOverride(bool InEnabled) { bStaticDisableCheckInChangesOverride = InEnabled; }
	static void SetSyncLatestDisabledOverride(bool InEnabled) { bStaticDisableSyncLatestOverride = InEnabled; }
	static FOnClicked& GetOnSyncLatestClickedStaticOverride() { return OnSyncLatestClickedStaticOverride; }

private:
	void OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider);
	void OnSourceControlStateChanged();
	
private:
	
	FOnClicked OnSyncLatestClicked;
	FOnClicked OnCheckInChangesClicked;

	TAttribute<bool> IsSyncLatestEnabled;
	TAttribute<bool> IsCheckInChangesEnabled;

	TAttribute<bool> IsSyncLatestSeparatorEnabled;
	TAttribute<bool> IsCheckInChangesSeparatorEnabled;

	/** Is there a conflict remaining? */
	bool bConflictsRemaining;
	int32 NumConflictsRemaining;

	FDelegateHandle SourceControlProviderChangedHandle;
	FDelegateHandle SourceControlStateChangedHandle;

	static bool bStaticDisableCheckInChangesOverride;
	static bool bStaticDisableSyncLatestOverride;
	static FOnClicked OnSyncLatestClickedStaticOverride;
};

#endif // SOURCE_CONTROL_WITH_SLATE