// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCoreSettings.h"

#include "Widgets/SChaosVDPlaybackViewport.h"


void UChaosVDSettingsObjectBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	BroadcastSettingsChanged();
}

void UChaosVDSettingsObjectBase::PostEditUndo()
{
	UObject::PostEditUndo();
	BroadcastSettingsChanged();
}

void UChaosVDSettingsObjectBase::BroadcastSettingsChanged()
{
	SettingsChangedDelegate.Broadcast(this);
}

void UChaosVDVisualizationSettingsObjectBase::BroadcastSettingsChanged()
{
	Super::BroadcastSettingsChanged();
	SChaosVDPlaybackViewport::ExecuteExternalViewportInvalidateRequest();
}

