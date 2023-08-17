// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class FReply;

enum class EChaosVDBrowseFileModalResponse
{
	OpenFolder,
	OpenTraceStore,
	Cancel
};

/**
 * 
 */
class SChaosBrowseTraceFileSourceModal : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SChaosBrowseTraceFileSourceModal)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Displays the dialog in a blocking fashion */
	EChaosVDBrowseFileModalResponse ShowModal();

protected:

	FReply OnButtonClick(EChaosVDBrowseFileModalResponse Response);

	EChaosVDBrowseFileModalResponse UserResponse = EChaosVDBrowseFileModalResponse::Cancel;
};
