// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDCoreSettings.h"

#include "Widgets/SChaosVDPlaybackViewport.h"


void UChaosVDSettingsObjectBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	BroadcastSettingsChanged(this);
}

void UChaosVDSettingsObjectBase::PostEditUndo()
{
	UObject::PostEditUndo();
	BroadcastSettingsChanged(this);
}

void UChaosVDSettingsObjectBase::BroadcastSettingsChanged(UObject* SettingsObject)
{
	SettingsChangedDelegate.Broadcast(this);
}

void UChaosVDVisualizationSettingsObjectBase::BroadcastSettingsChanged(UObject* SettingsObject)
{
	Super::BroadcastSettingsChanged(SettingsObject);
	SChaosVDPlaybackViewport::ExecuteExternalViewportInvalidateRequest();
}

