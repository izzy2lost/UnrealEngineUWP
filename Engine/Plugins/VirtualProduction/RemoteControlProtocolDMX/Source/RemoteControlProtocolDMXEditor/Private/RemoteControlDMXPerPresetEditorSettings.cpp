// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlDMXPerPresetEditorSettings.h"

#include "RemoteControlDMXUserData.h"

URemoteControlDMXPerPresetEditorSettings* URemoteControlDMXPerPresetEditorSettings::GetOrCreatePerPresetEditorSettings(URemoteControlPreset* Preset)
{
	URemoteControlDMXUserData* DMXUserData = URemoteControlDMXUserData::GetOrCreateDMXUserData(Preset);

	if (DMXUserData->PerPresetEditorSettings && DMXUserData->PerPresetEditorSettings->GetClass() == URemoteControlDMXPerPresetEditorSettings::StaticClass())
	{
		return CastChecked<URemoteControlDMXPerPresetEditorSettings>(DMXUserData->PerPresetEditorSettings);
	}
	else
	{
		DMXUserData->PerPresetEditorSettings = NewObject<URemoteControlDMXPerPresetEditorSettings>(DMXUserData, NAME_None);
		return CastChecked<URemoteControlDMXPerPresetEditorSettings>(DMXUserData->PerPresetEditorSettings);
	}
}
