// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkTypes.h"
#include "Widgets/SNullWidget.h"

/** Stub class for the recording controller. Will be replaced with the real implementation in a future CL. */
class FLiveLinkHubRecordingController
{
public:

	void Initialize(const TSharedPtr<class FLiveLinkHubPlaybackController>& InPlaybackController)
	{
	}

	~FLiveLinkHubRecordingController()
	{
	}

	TSharedRef<SWidget> MakeRecordToolbarEntry()
	{
		return SNullWidget::NullWidget;
	}

	void StartRecording()
	{
	}
	
	void StopRecording()
	{
	}

	bool IsRecording() const
	{
		return false;
	}
	
	void RecordStaticData(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, const FLiveLinkStaticDataStruct& StaticData)
	{
	}
	
	void RecordFrameData(const FLiveLinkSubjectKey& SubjectKey, const FLiveLinkFrameDataStruct& FrameData)
	{
	}
};
