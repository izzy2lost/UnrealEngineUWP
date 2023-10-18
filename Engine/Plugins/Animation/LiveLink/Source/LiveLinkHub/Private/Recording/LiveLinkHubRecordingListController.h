// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SNullWidget.h"

/** Stub class for the recording list controller. Will be replaced with the real implementation in a future CL. */
class FLiveLinkHubRecordingListController
{
public:
	FLiveLinkHubRecordingListController(const TSharedPtr<FLiveLinkHub>& InLiveLinkHub)
	{
	}

	/** Create the list's widget. */
	TSharedRef<SWidget> MakeRecordingList()
	{
		return SNullWidget::NullWidget;
	}
};
