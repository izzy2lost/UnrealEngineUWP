// Copyright Epic Games, Inc. All Rights Reserved.

#include "StormSyncCoreSettings.h"

UStormSyncCoreSettings::UStormSyncCoreSettings()
{
	IgnoredPackagesInternal.Add(TEXT("/Engine"));
	IgnoredPackagesInternal.Add(TEXT("/Script"));
	ExportDefaultNameFormatString = TEXT("%Y_%m_%d_%H%M%S");
}

FName UStormSyncCoreSettings::GetCategoryName() const
{
	return PluginCategoryName;
}

#if WITH_EDITOR
FText UStormSyncCoreSettings::GetSectionText() const
{
	return NSLOCTEXT("StormSyncCorePlugin", "StormSyncCoreSettingsSection", "Core Settings");
}
#endif
