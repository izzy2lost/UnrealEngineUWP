
/*=============================================================================
	UWPTargetSettings.h: Declares the UUWPTargetSettings class.
=============================================================================*/

#pragma once

#include "UWPTargetSettings.generated.h"

/**
 * Implements the settings for the UWP target platform.
 */
UCLASS(config = Engine, defaultconfig)
class UWPTARGETPLATFORM_API UUWPTargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category = Rendering)
	TArray<FString> TargetedRHIs;

	/**
	* Service Configuration GUID. Must match XDP configuration.
	*/
	UPROPERTY(EditAnywhere, config, Category = XboxLive)
	FString ServiceConfigId;

	/**
	* Title ID. Must match XDP configuration.
	*/
	UPROPERTY(EditAnywhere, config, Category = XboxLive)
	FString TitleId;

	UPROPERTY(EditAnywhere, config, Category=Packaging)
	FString SigningCertificate;

	UPROPERTY(EditAnywhere, config, Category=Packaging)
	FString Logo;

	UPROPERTY(EditAnywhere, config, Category=Packaging)
	FString SmallLogo;

	UPROPERTY(EditAnywhere, config, Category=Packaging)
	FString WideLogo;

	UPROPERTY(EditAnywhere, config, Category=Packaging)
	FString SplashScreen;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString TileBackgroundColorHex;

	UPROPERTY(EditAnywhere, config, Category = Packaging)
	FString SplashScreenBackgroundColorHex;

	UPROPERTY(EditAnywhere, Category = Packaging, meta=(HideAlphaChannel))
	FColor TileBackgroundColor;

	UPROPERTY(EditAnywhere, Category = Packaging, meta=(HideAlphaChannel))
	FColor SplashScreenBackgroundColor;

	/**
	* Minimum version of the universal platform required to run this title.
	* It will not be possible to deploy the build on earlier versions.
	*/
	UPROPERTY(EditAnywhere, config, Category = "OS Info", Meta = (DisplayName = "Minimum supported platform version"))
	FString MinimumPlatformVersion;

	/**
	* Specifies the maximum version of the universal platform on which the
	* title is known to work as expected.  When deployed to later versions
	* the title will experience the runtime behavior of the version given here.
	*/
	UPROPERTY(EditAnywhere, config, Category = "OS Info", Meta = (DisplayName = "Maximum tested platform version"))
	FString MaximumPlatformVersionTested;
};
