#include "PropertyEditorModule.h"
#include "UWPTargetSettingsCustomization.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "UWPTargetSettings.h"


#define LOCTEXT_NAMESPACE "FUWPPlatformEditorModule"

/**
 * Module for the UWP platform editor module.
 */
class FUWPPlatformEditorModule 
	: public IModuleInterface
{
public:

	/** Default constructor. */
	FUWPPlatformEditorModule( )
	{ }

	/** Destructor. */
	~FUWPPlatformEditorModule( )
	{
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "UWP",
				LOCTEXT("TargetSettingsName", "UWP"),
				LOCTEXT("TargetSettingsDescription", "Settings for Universal Windows Platform"),
				GetMutableDefault<UUWPTargetSettings>()
			);
		}

		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"UWPTargetSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FUWPTargetSettingsCustomization::MakeInstance)
			);
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "UWP");
		}
	}

private:

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FUWPPlatformEditorModule, UWPPlatformEditor);

