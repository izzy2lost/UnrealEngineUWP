// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaMediaDefines.h"
#include "AvaMediaEditorStyle.h"

class FAvaMediaEditorUtils
{
public:
	static const FSlateBrush* GetChannelStatusBrush(EAvaChannelState InChannelState, EAvaMediaIssueSeverity InChannelIssueSeverity);
	static FText GetChannelStatusText(EAvaChannelState InChannelState, EAvaMediaIssueSeverity InChannelIssueSeverity);
};
