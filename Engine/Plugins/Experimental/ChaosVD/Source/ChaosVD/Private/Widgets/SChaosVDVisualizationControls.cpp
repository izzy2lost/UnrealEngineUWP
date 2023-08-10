// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDVisualizationControls.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackViewportClient.h"
#include "ChaosVDRecording.h"
#include "ChaosVDScene.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SChaosVDNameListPicker.h"
#include "Widgets/SChaosVDPlaybackViewport.h"
#include "Widgets/Text/STextBlock.h"

void SChaosVDVisualizationControls::Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController,const TWeakPtr<SChaosVDPlaybackViewport>& InPlaybackViewport)
{
	RegisterNewController(InPlaybackController);

	PlaybackViewport = InPlaybackViewport;
	
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	TSharedRef<IDetailsView> DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	DetailsPanel->SetObject(GetMutableDefault<UChaosVDEditorSettings>());

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(2,5,2,5)
		.AutoHeight()
		[
			DetailsPanel
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2,5,2,5)
		[
			SNew(SExpandableArea)
				.InitiallyCollapsed(false)
				.BorderBackgroundColor(FLinearColor::White)
				.Padding(FMargin(8.f))
				.Visibility_Raw(this, &SChaosVDVisualizationControls::GetTrackedDataOptionsVisibility)
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
						.Text(FText::FromString("Trackable Data"))
						.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
					]
				]
				.BodyContent()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
						.Visibility_Raw(this, &SChaosVDVisualizationControls::GetTransformsPickerVisibility)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Transform to Track"))
							.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
						]
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						[
							SAssignNew(TransformNamePickerWidget, SChaosVDNameListPicker)
							.OnNameSleceted_Raw(this, &SChaosVDVisualizationControls::HandleTransformNameSelected)
						]	
					]
					+SVerticalBox::Slot()
					.Padding(2,5,2,5)
					[
						SNew(SHorizontalBox)
						.Visibility_Raw(this, &SChaosVDVisualizationControls::GetLocationsPickerVisibility)
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(FText::FromString("Location to Track"))
							.Font(FCoreStyle::Get().GetFontStyle("ExpandableArea.TitleFont"))
						]
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Right)
						[
							SAssignNew(LocationsNamePickerWidget, SChaosVDNameListPicker)
							.OnNameSleceted_Raw(this, &SChaosVDVisualizationControls::HandleLocationNameSelected)
						]
					]
				]
		]
	];
}

void SChaosVDVisualizationControls::HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid)
{
	if (UpdatedTrackInfo == nullptr)
	{
		return;
	}

	if (UpdatedTrackInfo->TrackType != EChaosVDTrackType::Game)
	{
		return;
	}

	const TSharedPtr<SChaosVDPlaybackViewport> PlaybackViewportSharedPtr = PlaybackViewport.Pin();
	const TSharedPtr<FChaosVDPlaybackController> ControllerSharedPtr = InController.Pin();
		
	if (!PlaybackViewportSharedPtr.IsValid() || !ControllerSharedPtr.IsValid())
	{
		return;
	}

	if (const TSharedPtr<FChaosVDRecording> RecordingSharedPtr  = ControllerSharedPtr->GetCurrentRecording().Pin())
	{
		if (FChaosVDGameFrameData* FrameData = RecordingSharedPtr->GetGameFrameData_AssumesLocked(UpdatedTrackInfo->CurrentFrame))
		{
			UpdateNameListFromKeys(FrameData->RecordedNonSolverTransformsByID, *TransformNamePickerWidget.Get());
			UpdateNameListFromKeys(FrameData->RecordedNonSolverLocationsByID, *LocationsNamePickerWidget.Get());
		}
	}
}

void SChaosVDVisualizationControls::HandleLocationNameSelected(TSharedPtr<FName> SelectedName)
{
	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->SelectedTrackedLocationName = SelectedName;
	}
}

void SChaosVDVisualizationControls::HandleTransformNameSelected(TSharedPtr<FName> SelectedName)
{
	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->SelectedTrackedTransformName = SelectedName;
	}
}

EVisibility SChaosVDVisualizationControls::GetLocationsPickerVisibility() const
{
	if (const UChaosVDEditorSettings* Settings = GetDefault<UChaosVDEditorSettings>())
	{
		return Settings->TrackingTarget == EChaosVDActorTrackingTarget::RecordedLocation ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}

EVisibility SChaosVDVisualizationControls::GetTransformsPickerVisibility() const
{
	if (const UChaosVDEditorSettings* Settings = GetDefault<UChaosVDEditorSettings>())
	{
		return Settings->TrackingTarget == EChaosVDActorTrackingTarget::RecordedTransform ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return EVisibility::Collapsed;
}


EVisibility SChaosVDVisualizationControls::GetTrackedDataOptionsVisibility() const
{
	const bool bIsTrackedDataVisible = GetLocationsPickerVisibility() == EVisibility::Visible ||  GetTransformsPickerVisibility() == EVisibility::Visible;

	return bIsTrackedDataVisible ? EVisibility::Visible : EVisibility::Collapsed;
}
