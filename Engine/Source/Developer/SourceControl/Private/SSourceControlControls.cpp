// Copyright Epic Games, Inc. All Rights Reserved.

#if SOURCE_CONTROL_WITH_SLATE

#include "SSourceControlControls.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "Misc/ConfigCacheIni.h"
#include "RevisionControlStyle/RevisionControlStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SSeparator.h"


#define LOCTEXT_NAMESPACE "SSkeinSourceControlWidgets"

/**
 * Construct this widget
 *
 * @param	InArgs	The declaration data for this widget
 */
void SSourceControlControls::Construct(const FArguments& InArgs)
{
	OnSyncLatestClicked = InArgs._OnClickedSyncLatest;
	OnCheckInChangesClicked = InArgs._OnClickedCheckInChanges;

	IsSyncLatestEnabled = InArgs._IsEnabledSyncLatest;
	IsCheckInChangesEnabled = InArgs._IsEnabledCheckInChanges;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot() // Check In Changes Button
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText(this, &SSourceControlControls::GetSourceControlCheckInStatusTooltipText)
			.Visibility(this, &SSourceControlControls::GetSourceControlCheckInStatusVisibility)
			.IsEnabled(this, &SSourceControlControls::IsSourceControlCheckInEnabled)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(this, &SSourceControlControls::GetSourceControlCheckInStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text(this, &SSourceControlControls::GetSourceControlCheckInStatusText)
				]
			]
			.OnClicked(this, &SSourceControlControls::OnSourceControlCheckInChangesClicked)
		]
		+SHorizontalBox::Slot() // Check In Kebab Combo button
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SComboButton)
			.ContentPadding(FMargin(7.f, 0.f))
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("StatusBar.StatusBarEllipsisComboButton"))
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.Visibility(this, &SSourceControlControls::GetSourceControlCheckInStatusVisibility)
			.OnGetMenuContent(InArgs._OnGenerateKebabMenu)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SSeparator)
			.Visibility(this, &SSourceControlControls::GetSourceControlCheckInStatusVisibility)
			.Thickness(1.0)
			.Orientation(EOrientation::Orient_Vertical)
		]
		+ SHorizontalBox::Slot() // Sync Latest Button
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("StatusBar.StatusBarButton"))
			.ToolTipText(this, &SSourceControlControls::GetSourceControlSyncStatusTooltipText)
			.Visibility(this, &SSourceControlControls::GetSourceControlSyncStatusVisibility)
			.IsEnabled(this, &SSourceControlControls::IsSourceControlSyncEnabled)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(this, &SSourceControlControls::GetSourceControlSyncStatusIcon)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(5, 0, 0, 0))
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text(this, &SSourceControlControls::GetSourceControlSyncStatusText)
				]
			]
			.OnClicked(this, &SSourceControlControls::OnSourceControlSyncClicked)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SSeparator)
			.Visibility(this, &SSourceControlControls::GetSourceControlCheckInStatusVisibility)
			.Thickness(1.0)
			.Orientation(EOrientation::Orient_Vertical)
		]
	];

	CheckSourceControlStatus();
}

void SSourceControlControls::CheckSourceControlStatus()
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();

	if (!SourceControlProviderChangedHandle.IsValid())
	{
		SourceControlProviderChangedHandle = SourceControlModule.RegisterProviderChanged(
			FSourceControlProviderChanged::FDelegate::CreateSP(this, &SSourceControlControls::OnSourceControlProviderChanged)
		);
		SourceControlStateChangedHandle = SourceControlModule.GetProvider().RegisterSourceControlStateChanged_Handle(
			FSourceControlStateChanged::FDelegate::CreateSP(this, &SSourceControlControls::OnSourceControlStateChanged)
		);
	}
}

void SSourceControlControls::OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	if (SourceControlStateChangedHandle.IsValid())
	{
		OldProvider.UnregisterSourceControlStateChanged_Handle(SourceControlStateChangedHandle);
		SourceControlStateChangedHandle.Reset();
	}

	if (!IsEngineExitRequested())
	{
		SourceControlStateChangedHandle = NewProvider.RegisterSourceControlStateChanged_Handle(
			FSourceControlStateChanged::FDelegate::CreateSP(this, &SSourceControlControls::OnSourceControlStateChanged)
		);
	}
}

void SSourceControlControls::OnSourceControlStateChanged()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<FSourceControlStateRef> Conflicts = SourceControlProvider.GetCachedStateByPredicate(
		[](const FSourceControlStateRef& State)
		{
			return State->IsConflicted();
		}
	);

	bConflictsRemaining = (Conflicts.Num() > 0);
}

bool SSourceControlControls::AreConflictsRemaining() const
{
	const bool bExiting = IsEngineExitRequested();

	if (!bExiting)
	{
		return bConflictsRemaining;
	}

	return false;
}

/** Sync Status */

bool SSourceControlControls::IsAtLatestRevision() const
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	return SourceControlModule.IsEnabled() &&
		SourceControlModule.GetProvider().IsAvailable() &&
		SourceControlModule.GetProvider().IsAtLatestRevision().IsSet() &&
		SourceControlModule.GetProvider().IsAtLatestRevision().GetValue();
}

bool SSourceControlControls::IsSourceControlSyncEnabled() const
{
	if (!HasSourceControlChangesToSync())
	{
		return false;
	}

	return IsSyncLatestEnabled.Get(true);
}

bool SSourceControlControls::HasSourceControlChangesToSync() const
{
	return !IsAtLatestRevision();
}

EVisibility SSourceControlControls::GetSourceControlSyncStatusVisibility() const
{
	if (!GIsEditor)
	{
		// Always visible in the Slate Viewer
		return EVisibility::Visible;
	}

	bool bDisplaySourceControlSyncStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlSyncStatus"), bDisplaySourceControlSyncStatus, GEditorIni);

	if (bDisplaySourceControlSyncStatus)
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled() &&
			SourceControlModule.GetProvider().IsAvailable() &&
			SourceControlModule.GetProvider().IsAtLatestRevision().IsSet()) // Only providers that implement IsAtLatestRevision are supported.
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

FText SSourceControlControls::GetSourceControlSyncStatusText() const
{
	if (HasSourceControlChangesToSync())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadText", "Sync Latest");
	}
	return LOCTEXT("SyncLatestButtonAtHeadText", "At Latest");
}

FText SSourceControlControls::GetSourceControlSyncStatusTooltipText() const
{
	if (AreConflictsRemaining())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadTooltipTextConflict", "Some of your local changes conflict with the latest snapshot of the project. Click here to review these conflicts.");
	}
	if (HasSourceControlChangesToSync())
	{
		return LOCTEXT("SyncLatestButtonNotAtHeadTooltipText", "Sync to the latest Snapshot for this project");
	}
	return LOCTEXT("SyncLatestButtonAtHeadTooltipText", "Currently at the latest Snapshot for this project");
}

const FSlateBrush* SSourceControlControls::GetSourceControlSyncStatusIcon() const
{
	static const FSlateBrush* ConflictBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.Conflicted");
	static const FSlateBrush* AtHeadBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.AtLatestRevision");
	static const FSlateBrush* NotAtHeadBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.NotAtLatestRevision");

	if (AreConflictsRemaining())
	{
		return ConflictBrush;
	}
	if (HasSourceControlChangesToSync())
	{
		return NotAtHeadBrush;
	}
	return AtHeadBrush;
}

FReply SSourceControlControls::OnSourceControlSyncClicked() const
{
	if (AreConflictsRemaining())
	{
		if (IConsoleObject* CObj = IConsoleManager::Get().FindConsoleObject(TEXT("UnrealRevisionControl.FocusConflictResolution")))
		{
			CObj->AsCommand()->Execute(/*Args=*/TArray<FString>(), /*InWorld=*/nullptr, *GLog);
		}
	}
	else if (HasSourceControlChangesToSync())
	{
		if (OnSyncLatestClicked.IsBound())
		{
			OnSyncLatestClicked.Execute();
		}
	}

	return FReply::Handled();
}

/** Check-in Status */

int SSourceControlControls::GetNumLocalChanges() const
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	if (SourceControlModule.IsEnabled() &&
		SourceControlModule.GetProvider().IsAvailable() &&
		SourceControlModule.GetProvider().GetNumLocalChanges().IsSet())
	{
		return SourceControlModule.GetProvider().GetNumLocalChanges().GetValue();
	}
	return 0;
}

bool SSourceControlControls::IsSourceControlCheckInEnabled() const
{
	if (!HasSourceControlChangesToCheckIn())
	{
		return false;
	}

	return IsCheckInChangesEnabled.Get(true);
}

bool SSourceControlControls::HasSourceControlChangesToCheckIn() const
{
	return (GetNumLocalChanges() > 0);
}

EVisibility SSourceControlControls::GetSourceControlCheckInStatusVisibility() const
{
	if (!GIsEditor)
	{
		// Always visible in the Slate Viewer
		return EVisibility::Visible;
	}

	bool bDisplaySourceControlCheckInStatus = false;
	GConfig->GetBool(TEXT("SourceControlSettings"), TEXT("DisplaySourceControlCheckInStatus"), bDisplaySourceControlCheckInStatus, GEditorIni);

	if (bDisplaySourceControlCheckInStatus)
	{
		ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
		if (SourceControlModule.IsEnabled() &&
			SourceControlModule.GetProvider().IsAvailable() &&
			SourceControlModule.GetProvider().GetNumLocalChanges().IsSet()) // Only providers that implement GetNumLocalChanges are supported.
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

FText SSourceControlControls::GetSourceControlCheckInStatusText() const
{
	if (HasSourceControlChangesToCheckIn())
	{
		return LOCTEXT("CheckInButtonChangesText", "Check-in Changes");
	}
	return LOCTEXT("CheckInButtonNoChangesText", "No Changes");
}


FText SSourceControlControls::GetSourceControlCheckInStatusTooltipText() const
{
	if (AreConflictsRemaining())
	{
		return LOCTEXT("CheckInButtonChangesTooltipTextConflict", "Some of your local changes conflict with the latest snapshot of the project. Click here to review these conflicts.");
	}
	if (HasSourceControlChangesToCheckIn())
	{
		return FText::Format(LOCTEXT("CheckInButtonChangesTooltipText", "Check-in {0} change(s) to this project"), GetNumLocalChanges());
	}
	return LOCTEXT("CheckInButtonNoChangesTooltipText", "No Changes to check in for this project");
}

const FSlateBrush* SSourceControlControls::GetSourceControlCheckInStatusIcon() const
{
	static const FSlateBrush* ConflictBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.Conflicted");
	static const FSlateBrush* NoLocalChangesBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.NoLocalChanges");
	static const FSlateBrush* HasLocalChangesBrush = FRevisionControlStyleManager::Get().GetBrush("RevisionControl.StatusBar.HasLocalChanges");

	if (AreConflictsRemaining())
	{
		return ConflictBrush;
	}
	if (HasSourceControlChangesToCheckIn())
	{
		return HasLocalChangesBrush;
	}
	return NoLocalChangesBrush;
}

FReply SSourceControlControls::OnSourceControlCheckInChangesClicked() const
{
	if (AreConflictsRemaining())
	{
		if (IConsoleObject* CObj = IConsoleManager::Get().FindConsoleObject(TEXT("UnrealRevisionControl.FocusConflictResolution")))
		{
			CObj->AsCommand()->Execute(/*Args=*/TArray<FString>(), /*InWorld=*/nullptr, *GLog);
		}
	}
	else if (HasSourceControlChangesToCheckIn())
	{
		if (OnCheckInChangesClicked.IsBound())
		{
			OnCheckInChangesClicked.Execute();
		}
	}

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE

#endif // SOURCE_CONTROL_WITH_SLATE