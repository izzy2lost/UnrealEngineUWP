// Copyright Epic Games, Inc. All Rights Reserved.


#include "StormSyncDrivesSettings.h"

UStormSyncDrivesSettings::UStormSyncDrivesSettings()
{
}

FName UStormSyncDrivesSettings::GetCategoryName() const
{
	return PluginCategoryName;
}

#if WITH_EDITOR
FText UStormSyncDrivesSettings::GetSectionText() const
{
	return NSLOCTEXT("StormSyncDrives", "StormSyncDrivesSettingsSection", "Mount Points Settings");
}
#endif
