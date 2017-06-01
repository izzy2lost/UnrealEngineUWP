

#include "UWPTargetSettings.h"
#include "Misc/ConfigCacheIni.h"


/* UUWPTargetSettings structors
 *****************************************************************************/

UUWPTargetSettings::UUWPTargetSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUWPTargetSettings::PostInitProperties()
{
	Super::PostInitProperties();

	// Copy across from old location.
	FString DefaultConfigFile = GetDefaultConfigFilename();
	if (GConfig->DoesSectionExist(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), DefaultConfigFile))
	{
		if (!GConfig->DoesSectionExist(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), DefaultConfigFile))
		{
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("Logo"), Logo, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SmallLogo"), SmallLogo, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("WideLogo"), WideLogo, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SplashScreen"), SplashScreen, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("StoreLogo"), StoreLogo, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SigningCertificate"), SigningCertificate, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TileBackgroundColorHex"), TileBackgroundColorHex, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SplashScreenBackgroundColorHex"), SplashScreenBackgroundColorHex, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TitleId"), TitleId, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("ServiceConfigId"), ServiceConfigId, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("MinimumPlatformVersion"), MinimumPlatformVersion, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("MaximumPlatformVersionTested"), MaximumPlatformVersionTested, DefaultConfigFile);
			UpdateDefaultConfigFile();
		}
		GConfig->EmptySection(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), DefaultConfigFile);
		if (!GConfig->DoesSectionExist(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), DefaultConfigFile))
		{
			UpdateDefaultConfigFile();
		}
	}

	TileBackgroundColor = FColor::FromHex(TileBackgroundColorHex);
	SplashScreenBackgroundColor = FColor::FromHex(SplashScreenBackgroundColorHex);

	// Determine if we need to set default capabilities for this project.
	GConfig->GetBool(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), TEXT("bSetDefaultCapabilities"), bSetDefaultCapabilities, DefaultConfigFile);
}