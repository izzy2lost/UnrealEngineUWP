
#include "UWPTargetPlatformPrivatePCH.h"
#include "AllowWindowsPlatformTypes.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"


#define LOCTEXT_NAMESPACE "FUWPTargetPlatformModule"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* UWPTargetSingleton = NULL;


/**
 * Module for the UWP target platform.
 */
class FUWPTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Default constructor. */
	FUWPTargetPlatformModule( )
	{ }

	/** Destructor. */
	~FUWPTargetPlatformModule( )
	{
	}

public:
	
	// ITargetPlatformModule interface
	
	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (UWPTargetSingleton == NULL)
		{
			//@todo UWP: Check for SDK?

			UWPTargetSingleton = new FUWPTargetPlatform();
		}
		
		return UWPTargetSingleton;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		TargetSettings = NewObject<UUWPTargetSettings>(GetTransientPackage(), "UWPTargetSettings", RF_Standalone);

		// We need to manually load the config properties here, as this module is loaded before the UObject system is setup to do this
		GConfig->GetArray(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TargetedRHIs"), TargetSettings->TargetedRHIs, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("Logo"), TargetSettings->Logo, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SmallLogo"), TargetSettings->SmallLogo, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("WideLogo"), TargetSettings->WideLogo, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SplashScreen"), TargetSettings->SplashScreen, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SigningCertificate"), TargetSettings->SigningCertificate, GEngineIni);
		if (GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TileBackgroundColorHex"), TargetSettings->TileBackgroundColorHex, GEngineIni))
		{
			TargetSettings->TileBackgroundColor = FColor::FromHex(TargetSettings->TileBackgroundColorHex);
		}
		if (GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SplashScreenBackgroundColorHex"), TargetSettings->SplashScreenBackgroundColorHex, GEngineIni))
		{
			TargetSettings->SplashScreenBackgroundColor = FColor::FromHex(TargetSettings->SplashScreenBackgroundColorHex);
		}
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TitleId"), TargetSettings->TitleId, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("ServiceConfigId"), TargetSettings->ServiceConfigId, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("MinimumPlatformVersion"), TargetSettings->MinimumPlatformVersion, GEngineIni);
		GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("MaximumPlatformVersionTested"), TargetSettings->MaximumPlatformVersionTested, GEngineIni);

		TargetSettings->AddToRoot();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "UWP",
				LOCTEXT("TargetSettingsName", "UWP"),
				LOCTEXT("TargetSettingsDescription", "Settings for Universal Windows Platform"),
				TargetSettings
				);
		}
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

	// Holds the target settings.
	UUWPTargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FUWPTargetPlatformModule, UWPTargetPlatform);


#include "HideWindowsPlatformTypes.h"
