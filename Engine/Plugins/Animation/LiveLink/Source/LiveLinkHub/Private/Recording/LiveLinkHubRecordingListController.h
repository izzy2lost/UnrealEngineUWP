// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "IContentBrowserSingleton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkHubPlaybackController.h"
#include "LiveLinkHubRecordingListController.h"
#include "SLiveLinkHubRecordingListView.h"
#include "LiveLinkRecording.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub.RecordingList"

class FLiveLinkHubRecordingListController
{
public:
	FLiveLinkHubRecordingListController(const TSharedRef<FLiveLinkHub>& InLiveLinkHub)
		: LiveLinkHub(InLiveLinkHub)
	{
	}

	/** Create the list's widget. */
	TSharedRef<SWidget> MakeRecordingList()
	{
		return SNew(SLiveLinkHubRecordingListView)
			.IsLooping_Raw(this, &FLiveLinkHubRecordingListController::IsLooping)
			.OnSetLooping_Raw(this, &FLiveLinkHubRecordingListController::SetLooping)
			.OnImportRecording_Raw(this, &FLiveLinkHubRecordingListController::OnImportRecording);
	}

private:
	/** Returns whether the recording playback should loop. */
	bool IsLooping() const
	{
		if (TSharedPtr<FLiveLinkHub> HubPtr = LiveLinkHub.Pin())
		{
			return HubPtr->GetPlaybackController()->IsLooping();
		}
		return false;
	}

	/** Sets whether the playback should loop. */
	void SetLooping(bool bLooping) const
	{
		if (TSharedPtr<FLiveLinkHub> HubPtr = LiveLinkHub.Pin())
		{
			HubPtr->GetPlaybackController()->SetLooping(bLooping);
		}
	}

	/** Handler called when a recording a clicked to start the recording.  */
	void OnImportRecording(const FAssetData& AssetData) const
	{
		UObject* RecordingAssetData = AssetData.GetAsset();
		if (!RecordingAssetData)
		{
			UE_LOG(LogLiveLinkHub, Warning, TEXT("Failed to import recording %s"), *AssetData.AssetName.ToString());
			StopPlayback();
			return;
		}

		ULiveLinkRecording* ImportedRecording = Cast<ULiveLinkRecording>(RecordingAssetData);
		LiveLinkHub.Pin()->GetPlaybackController()->PlayRecording(ImportedRecording);
	}

	/** Handler called when user clicks in the recording list to stop the current recording (Temporary until we have a stop button).  */
	void StopPlayback() const
	{
		if (TSharedPtr<FLiveLinkHub> HubPtr = LiveLinkHub.Pin())
		{
			HubPtr->GetPlaybackController()->StopPlayback();
		}
	}

private:
	/** LiveLinkHub object that holds the different controllers. */
	TWeakPtr<FLiveLinkHub> LiveLinkHub;
};

#undef LOCTEXT_NAMESPACE /* LiveLinkHub.RecordingList */
