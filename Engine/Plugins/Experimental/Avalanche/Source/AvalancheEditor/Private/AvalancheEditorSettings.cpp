// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheEditorSettings.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Viewport/AvaCineCameraActor.h"

#define LOCTEXT_NAMESPACE "AvalancheEditorSettings"

UAvalancheEditorSettings::UAvalancheEditorSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName = TEXT("Editor");
}

UAvalancheEditorSettings* UAvalancheEditorSettings::Get()
{
	UAvalancheEditorSettings* DefaultSettings = GetMutableDefault<UAvalancheEditorSettings>();
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		DefaultSettings->SetFlags(RF_Transactional);
	}
	return DefaultSettings;
}

void UAvalancheEditorSettings::OpenEditorSettingsWindow() const
{
	static ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
	SettingsModule.ShowViewer(GetContainerName(), CategoryName, SectionName);
}

void UAvalancheEditorSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (const FName PresetNameNoLumen = TEXT("No Lumen");
		!ViewportQualityPresets.Contains(PresetNameNoLumen))
	{
		ViewportQualityPresets.Add(PresetNameNoLumen, FAvaViewportQualitySettings::Preset(PresetNameNoLumen));
	}

	if (const FName PresetNameReduced = TEXT("Reduced");
		!ViewportQualityPresets.Contains(PresetNameReduced))
	{
		ViewportQualityPresets.Add(PresetNameReduced, FAvaViewportQualitySettings::Preset(PresetNameReduced));
	}

	// Verifying integrity and sorting will ensure any new/removed entries are handled correctly.
	DefaultViewportQualitySettings.VerifyIntegrity();
	DefaultViewportQualitySettings.SortFeaturesByDisplayText();

	for (TPair<FName, FAvaViewportQualitySettings>& Preset : ViewportQualityPresets)
	{
		FAvaViewportQualitySettings& Settings = Preset.Value;
		Settings.VerifyIntegrity();
		Settings.SortFeaturesByDisplayText();
	}
}

void UAvalancheEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnChanged.Broadcast(this, PropertyChangedEvent.GetPropertyName());
}

// todo: when the reset to defaults issue will be solved, this function could be used from both PostInitProperties and PECP to update AAvaCineCameraActor defaults
void UAvalancheEditorSettings::UpdateAvaCineCameraDefaults() const
{
	AAvaCineCameraActor::SetDefaultCameraDistance(CameraDistance);
}

#undef LOCTEXT_NAMESPACE
