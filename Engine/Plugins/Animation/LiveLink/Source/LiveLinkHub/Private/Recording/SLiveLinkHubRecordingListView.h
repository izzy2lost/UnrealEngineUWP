// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "ContentBrowserModule.h"
#include "Delegates/DelegateCombinations.h"
#include "LiveLinkHub.h"
#include "Recording/LiveLinkRecording.h"
#include "Recording/LiveLinkHubPlaybackController.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"


#define LOCTEXT_NAMESPACE "LiveLinkHub.RecordingListView"

DECLARE_DELEGATE_RetVal(bool, FOnGetLooping);
DECLARE_DELEGATE_OneParam(FOnSetLooping, bool);
DECLARE_DELEGATE_OneParam(FOnImportRecording, const struct FAssetData&);

class SLiveLinkHubRecordingListView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveLinkHubRecordingListView)
		{}
		SLATE_EVENT(FOnGetLooping, IsLooping)
		SLATE_EVENT(FOnSetLooping, OnSetLooping)
		SLATE_EVENT(FOnImportRecording, OnImportRecording)
	SLATE_END_ARGS()

	//~ Begin SWidget interface
	void Construct(const FArguments& InArgs)
	{
		OnGetLoopingDelegate = InArgs._IsLooping;
		OnSetLoopingDelegate = InArgs._OnSetLooping;
		OnImportRecordingDelegate = InArgs._OnImportRecording;

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				CreateRecordingPicker()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LoopRecordingLabel", "Loop"))
				]
				+ SHorizontalBox::Slot()
				.Padding(4.f, 0)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SCheckBox)
					.Padding(FMargin(4.0f, 0.0f))
					.IsChecked(this, &SLiveLinkHubRecordingListView::IsLoopedChecked)
					.OnCheckStateChanged(this, &SLiveLinkHubRecordingListView::OnCheckStateChanged)
				]
			]
		];
	}
	//~ End SWidget interface

private:
	/** Returns whether the looping checkbox should be checked or unchecked. */
	ECheckBoxState IsLoopedChecked() const
	{
		return OnGetLoopingDelegate.Execute() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Handler to change the looping option for playback. */
	void OnCheckStateChanged(ECheckBoxState InState)
	{
		OnSetLoopingDelegate.Execute(InState == ECheckBoxState::Checked);
	}

	/** Callback to notice the hub that we've selected a recording to play. */
	void OnImportRecording(const FAssetData& AssetData) const
	{
		OnImportRecordingDelegate.Execute(AssetData);
	}

	/** Creates the asset picker widget for selecting a recording. */
	TSharedRef<SWidget> CreateRecordingPicker()
	{
		FMenuBuilder MenuBuilder(true, nullptr);

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.SelectionMode = ESelectionMode::Single;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
			AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.bShowBottomToolbar = true;
			AssetPickerConfig.bAutohideSearchBar = false;
			AssetPickerConfig.bAllowDragging = false;
			AssetPickerConfig.bCanShowClasses = false;
			AssetPickerConfig.bShowPathInColumnView = true;
			AssetPickerConfig.bSortByPathInColumnView = false;
			AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoRecordings_Warning", "No Recordings Found");

			AssetPickerConfig.bForceShowEngineContent = true;
			AssetPickerConfig.bForceShowPluginContent = true;

			AssetPickerConfig.Filter.ClassPaths.Add(ULiveLinkRecording::StaticClass()->GetClassPathName());
			AssetPickerConfig.Filter.bRecursiveClasses = true;
			AssetPickerConfig.Filter.bRecursivePaths = true;
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &SLiveLinkHubRecordingListView::OnImportRecording);
		}

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ImportRecording_MenuSection", "Import Recording"));
		{
			TSharedRef<SWidget> PresetPicker = SNew(SBox)
				.MinDesiredWidth(400.f)
				.MinDesiredHeight(400.f)
				[
					ContentBrowser.CreateAssetPicker(AssetPickerConfig)
				];

			MenuBuilder.AddWidget(PresetPicker, FText(), true, false);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

private:
	/** Delegate for checking if the recording playback should loop. */
	FOnGetLooping OnGetLoopingDelegate;
	/** Delegate to set the looping option. */
	FOnSetLooping OnSetLoopingDelegate;
	/** Delegate used for noticing the hub that a recording was selected for playback. */
	FOnImportRecording OnImportRecordingDelegate;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.RecordingListView */
