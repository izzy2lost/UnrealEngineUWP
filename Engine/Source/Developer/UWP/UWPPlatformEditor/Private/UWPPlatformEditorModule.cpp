
#include "UWPPlatformEditorPrivatePCH.h"
#include "PropertyEditorModule.h"
#include "UWPTargetSettingsCustomization.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


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
		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"UWPTargetSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FUWPTargetSettingsCustomization::MakeInstance)
			);
	}

	virtual void ShutdownModule() override
	{

	}

private:

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FUWPPlatformEditorModule, UWPPlatformEditor);

