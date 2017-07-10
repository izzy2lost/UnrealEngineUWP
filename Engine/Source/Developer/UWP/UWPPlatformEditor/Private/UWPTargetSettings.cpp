

#include "UWPTargetSettings.h"
#include "Misc/ConfigCacheIni.h"


/* UUWPTargetSettings structors
 *****************************************************************************/

UUWPTargetSettings::UUWPTargetSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnablePIXProfiling(1)
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
			FString OldTileBackgroundColorHex;
			FString OldSplashScreenBackgroundColorHex;
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TileBackgroundColor"), OldTileBackgroundColorHex, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("SplashScreenBackgroundColor"), OldSplashScreenBackgroundColorHex, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("TitleId"), TitleId, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("ServiceConfigId"), ServiceConfigId, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("MinimumPlatformVersion"), MinimumPlatformVersion, DefaultConfigFile);
			GConfig->GetString(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), TEXT("MaximumPlatformVersionTested"), MaximumPlatformVersionTested, DefaultConfigFile);

			TileBackgroundColor = FColor::FromHex(OldTileBackgroundColorHex);
			SplashScreenBackgroundColor = FColor::FromHex(OldSplashScreenBackgroundColorHex);

			UpdateDefaultConfigFile();
		}
		GConfig->EmptySection(TEXT("/Script/UWPTargetPlatform.UWPTargetSettings"), DefaultConfigFile);
		if (!GConfig->DoesSectionExist(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), DefaultConfigFile))
		{
			UpdateDefaultConfigFile();
		}
	}

	// Determine if we need to set default capabilities for this project.
	GConfig->GetBool(TEXT("/Script/UWPPlatformEditor.UWPTargetSettings"), TEXT("bSetDefaultCapabilities"), bSetDefaultCapabilities, DefaultConfigFile);
}