// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixDspEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

#include "IAssetTools.h"
#include "ISettingsModule.h"

#include "PropertyEditorModule.h"
#include "Customization/TypedParameterCustomization.h"
#include "Customization/PitchShifterNameCustomization.h"
#include "Customization/PitchShifterConfigCustomization.h"
#include "Customization/PannerDetailsCustomization.h"
#include "Customization/FusionPatchDetailCustomization.h"
#include "HarmonixDsp/FusionSampler/FusionPatch.h"

#define LOCTEXT_NAMESPACE "HarmonixDspEditor"

DEFINE_LOG_CATEGORY(LogHarmonixDspEditor)

void FHarmonixDspEditorModule::StartupModule()
{
	// Register property customizations
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.RegisterCustomPropertyTypeLayout("TypedParameter", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FTypedParameterCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout("PitchShifterName", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPitchShifterNameCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomPropertyTypeLayout("PannerDetails", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPannerDetailsCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout("StretcherAndPitchShifterFactoryConfig", FOnGetDetailCustomizationInstance::CreateStatic(&FPitchShifterConfigCustomization::MakeInstance));
	PropertyEditorModule.RegisterCustomClassLayout(UFusionPatch::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FFusionPatchDetailCustomization::MakeInstance));
}

void FHarmonixDspEditorModule::ShutdownModule()
{

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditorModule.UnregisterCustomClassLayout(UFusionPatch::StaticClass()->GetFName());
	PropertyEditorModule.UnregisterCustomClassLayout("StretcherAndPitchShifterFactoryConfig");
	PropertyEditorModule.UnregisterCustomPropertyTypeLayout("PannerDetails");
	PropertyEditorModule.UnregisterCustomPropertyTypeLayout("PitchShifterName");
	PropertyEditorModule.UnregisterCustomPropertyTypeLayout("TypedParameter");
}

IMPLEMENT_MODULE(FHarmonixDspEditorModule, HarmonixDspEditor);

#undef LOCTEXT_NAMESPACE
