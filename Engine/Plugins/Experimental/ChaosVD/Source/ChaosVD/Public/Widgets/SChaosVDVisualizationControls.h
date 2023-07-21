// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDPlaybackControllerObserver.h"
#include "Widgets/SChaosVDNameListPicker.h"
#include "Templates/SharedPointer.h"

class SChaosVDPlaybackViewport;
class FChaosVDPlaybackViewportClient;
struct FChaosVDTrackInfo;
struct FGuid;

/**
 * Widget containing the visualization controls for the Chaos Visual debugger tool to be shown in a Tab
 */
class SChaosVDVisualizationControls : public SCompoundWidget, public FChaosVDPlaybackControllerObserver
{
public:
	SLATE_BEGIN_ARGS( SChaosVDVisualizationControls ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController, const TWeakPtr<SChaosVDPlaybackViewport>& InPlaybackViewport);

	virtual void HandleControllerTrackFrameUpdated(TWeakPtr<FChaosVDPlaybackController> InController, const FChaosVDTrackInfo* UpdatedTrackInfo, FGuid InstigatorGuid) override;

protected:

	void HandleLocationNameSelected(TSharedPtr<FName> SelectedName);
	void HandleTransformNameSelected(TSharedPtr<FName> SelectedName);

	template<typename ValueType>
	void UpdateNameListFromKeys(const TMap<FName, ValueType>& MapToEvaluate, SChaosVDNameListPicker& NameList);
	
	EVisibility GetLocationsPickerVisibility() const;
	EVisibility GetTransformsPickerVisibility() const;
	EVisibility GetTrackedDataOptionsVisibility() const;

	TSharedPtr<SChaosVDNameListPicker> TransformNamePickerWidget;
	
	TSharedPtr<SChaosVDNameListPicker> LocationsNamePickerWidget;

	TWeakPtr<SChaosVDPlaybackViewport> PlaybackViewport;

	TSharedPtr<FName> SelectedTrackedTransformName;
	TSharedPtr<FName> SelectedTrackedLocationName;
};

template <typename ValueType>
void SChaosVDVisualizationControls::UpdateNameListFromKeys(const TMap<FName, ValueType>& MapToEvaluate, SChaosVDNameListPicker& NameList)
{
	TArray<TSharedPtr<FName>> NewNameList;
	NewNameList.Reserve(MapToEvaluate.Num());

	Algo::Transform(MapToEvaluate, NewNameList, [](const TPair<FName, ValueType>& TransformData){ return MakeShared<FName>(TransformData.Key);});
	
	NameList.UpdateNameList(MoveTemp(NewNameList));
}
