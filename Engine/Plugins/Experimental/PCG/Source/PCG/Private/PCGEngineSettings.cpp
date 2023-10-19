// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEngineSettings.h"

#define LOCTEXT_NAMESPACE "PCGEngineSettings"

FName UPCGEngineSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UPCGEngineSettings::GetSectionText() const
{
	return LOCTEXT("PCGEngineSettingsName", "PCG");
}
#endif // WITH_EDITOR

void UPCGEngineSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UPCGEngineSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE