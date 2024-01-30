// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMediaEditorUtils.h"

const FSlateBrush* FAvaMediaEditorUtils::GetChannelStatusBrush(EAvaChannelState InChannelState, EAvaMediaIssueSeverity InChannelIssueSeverity)
{
	if (InChannelState == EAvaChannelState::Live)
	{
		switch (InChannelIssueSeverity)
		{
		case EAvaMediaIssueSeverity::None:
			return FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.BroadcastLive");
		case EAvaMediaIssueSeverity::Warnings:
			return FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.BroadcastWarning");
		case EAvaMediaIssueSeverity::Errors:
			return FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.BroadcastError");
		}
	}
	else if (InChannelState == EAvaChannelState::Idle)
	{
		return FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.BroadcastIdle");
	}
	else
	{
		return FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.BroadcastOffline");
	}
	
	return FAvaMediaEditorStyle::Get().GetBrush("AvalancheMediaEditor.BroadcastIdle");
}

FText FAvaMediaEditorUtils::GetChannelStatusText(EAvaChannelState InChannelState, EAvaMediaIssueSeverity InChannelIssueSeverity)
{
	//TODO: Possibly also add Issue Severity (if not none)
	return StaticEnum<EAvaChannelState>()->GetDisplayNameTextByIndex(static_cast<int32>(InChannelState));
}
