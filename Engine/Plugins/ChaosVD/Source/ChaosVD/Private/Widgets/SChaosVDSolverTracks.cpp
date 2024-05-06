// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDSolverTracks.h"

#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "Widgets/SChaosVDSolverPlaybackControls.h"
#include "Widgets/Layout/SExpandableArea.h"

void SChaosVDSolverTracks::Construct(const FArguments& InArgs, TWeakPtr<FChaosVDPlaybackController> InPlaybackController)
{
	ChildSlot
	[
		SAssignNew(SolverTracksListWidget, SListView<TSharedPtr<const FChaosVDTrackInfo>>)
		.ListItemsSource(&CachedTrackInfoArray)
		.SelectionMode( ESelectionMode::None )
		.ListViewStyle(&FAppStyle::Get().GetWidgetStyle<FTableViewStyle>("SimpleListView"))
		.OnGenerateRow(this, &SChaosVDSolverTracks::MakeSolverTrackControlsFromTrackInfo)
	];

	ensure(InPlaybackController.IsValid());

	RegisterNewController(InPlaybackController);
	
	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = InPlaybackController.Pin())
	{
		if (const TSharedPtr<const FChaosVDTrackInfo> GameTrackInfo = CurrentPlaybackControllerPtr->GetTrackInfo(EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID))
		{
			HandleControllerTrackFrameUpdated(InPlaybackController, GameTrackInfo.ToSharedRef(), InvalidGuid);
		}
	}
	else
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Solver tracks contructed with an invalid player controler. The solver tracks widget will not be functional"), ANSI_TO_TCHAR(__FUNCTION__))	
	}
}

void SChaosVDSolverTracks::HandlePlaybackControllerDataUpdated(TWeakPtr<FChaosVDPlaybackController> InPlaybackController)
{
	if (PlaybackController != InPlaybackController)
	{
		RegisterNewController(InPlaybackController);
	}

	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = InPlaybackController.Pin())
	{
		// If the controller data was updated, need to update our cache track info data as it could have been changed.
		// For example this can happen when we load another recording. We use the GameTrack info for that as it is the one that is always valid
		if (const TSharedPtr<const FChaosVDTrackInfo> GameTrackInfo = CurrentPlaybackControllerPtr->GetTrackInfo(EChaosVDTrackType::Game, FChaosVDPlaybackController::GameTrackID))
		{
			UpdatedCachedTrackInfoData(InPlaybackController, GameTrackInfo.ToSharedRef());
		}
	}
}

void SChaosVDSolverTracks::UpdatedCachedTrackInfoData(TWeakPtr<FChaosVDPlaybackController> InPlaybackController, const TSharedRef<const FChaosVDTrackInfo>& UpdatedTrackInfo)
{
	if (const TSharedPtr<FChaosVDPlaybackController> CurrentPlaybackControllerPtr = InPlaybackController.Pin())
	{
		TArray<TSharedPtr<const FChaosVDTrackInfo>> TrackInfoArray;
		CurrentPlaybackControllerPtr->GetAvailableTrackInfosAtTrackFrame(EChaosVDTrackType::Solver, UpdatedTrackInfo, TrackInfoArray);

		if (TrackInfoArray != CachedTrackInfoArray)
		{
			CachedTrackInfoArray = MoveTemp(TrackInfoArray);
			SolverTracksListWidget->RebuildList();
		}
	}
	else
	{
		CachedTrackInfoArray.Empty();
		SolverTracksListWidget->RebuildList();
	}
}

void SChaosVDSolverTracks::HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InPlaybackController, TWeakPtr<const FChaosVDTrackInfo> UpdatedTrackInfo, FGuid InstigatorGuid)
{
	if (InstigatorGuid == GetInstigatorID())
	{
		// Ignore the update if we initiated it
		return;
	}

	TSharedPtr<const FChaosVDTrackInfo> UpdatedTrackInfoPtr = UpdatedTrackInfo.Pin();

	if (!UpdatedTrackInfoPtr)
	{
		return;
	}

	// Only Game Frame Track Update can change the available solvers
	if (UpdatedTrackInfoPtr->TrackType == EChaosVDTrackType::Solver)
	{
		return;
	}

	UpdatedCachedTrackInfoData(InPlaybackController, UpdatedTrackInfoPtr.ToSharedRef());
}

TSharedRef<ITableRow> SChaosVDSolverTracks::MakeSolverTrackControlsFromTrackInfo(TSharedPtr<const FChaosVDTrackInfo> TrackInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> RowWidget;
	if (TrackInfo)
	{
		RowWidget = SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10.0f, 2.0f, 10.0f, 0.0f)
				[
					SNew(SExpandableArea)
						.InitiallyCollapsed(false)
						.BorderBackgroundColor(FLinearColor::White)
						.Padding(FMargin(8.f))
						.HeaderContent()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.AutoWidth()
							.Padding(0.f, 0.f, 0.f, 0.f)
							[
								SNew(STextBlock)
								.Text(FText::FromName(TrackInfo->TrackName))
								.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
							]
						]
						.BodyContent()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.Padding(2.f, 4.f, 2.f, 12.f)
							[
								SNew(SChaosVDSolverPlaybackControls, TrackInfo.ToSharedRef(), PlaybackController)
							]
						]
				];
	}
	else
	{
		RowWidget = SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ChaosVisualDebugger", "SolverPlaybackControlsErrorMessage", "Failed to read data for solver."))
		];
	}

	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		[
			RowWidget.ToSharedRef()
		];
}
