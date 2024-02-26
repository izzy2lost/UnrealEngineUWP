// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSTargetPlatformSettings.cpp: Implements the FIOSTargetPlatformSettings class.
=============================================================================*/

#include "IOSTargetPlatformSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

#if WITH_ENGINE
#include "Engine/Texture.h"
#include "TextureResource.h"
#endif

/* FIOSTargetPlatformSettings structors
 *****************************************************************************/

FIOSTargetPlatformSettings::FIOSTargetPlatformSettings(bool bInIsTVOS, bool bInIsVisionOS)
	// override the ini name up in the base classes, which will go into the FTargetPlatformInfo
	: TTargetPlatformSettingsBase()
	, bIsTVOS(bInIsTVOS)
	, bIsVisionOS(bInIsVisionOS)
	, MobileShadingPath(0)
	, bDistanceField(false)
	, bMobileForwardEnableClusteredReflections(false)
	, bMobileVirtualTextures(false)
{
#if WITH_ENGINE
	TextureLODSettings = nullptr; // TextureLODSettings are registered by the device profile.
	StaticMeshLODSettings.Initialize(this);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.DistanceFields"), bDistanceField, GEngineIni);
	GetConfigSystem()->GetInt(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.ShadingPath"), MobileShadingPath, GEngineIni);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.Forward.EnableClusteredReflections"), bMobileForwardEnableClusteredReflections, GEngineIni);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.VirtualTextures"), bMobileVirtualTextures, GEngineIni);
#endif // #if WITH_ENGINE
}


FIOSTargetPlatformSettings::~FIOSTargetPlatformSettings()
{
}

/* ITargetPlatform interface
 *****************************************************************************/
static bool SupportsMetal()
{
	// default to NOT supporting metal
	bool bSupportsMetal = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetal"), bSupportsMetal, GEngineIni);
	return bSupportsMetal;
}

static bool SupportsMetalMRT()
{
	// default to NOT supporting metal MRT
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);
	return bSupportsMetalMRT;
}

static bool SupportsA8Devices()
{
    // default to NOT supporting A8 devices
    bool bSupportAppleA8 = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportAppleA8"), bSupportAppleA8, GEngineIni);
    return bSupportAppleA8;
}

bool FIOSTargetPlatformSettings::SupportsFeature( ETargetPlatformFeatures Feature ) const
{
	switch (Feature)
	{
		case ETargetPlatformFeatures::Packaging:
		case ETargetPlatformFeatures::DeviceOutputLog:
			return true;

		case ETargetPlatformFeatures::MobileRendering:
		case ETargetPlatformFeatures::LowQualityLightmaps:
			return SupportsMetal();
			
		case ETargetPlatformFeatures::DeferredRendering:
		case ETargetPlatformFeatures::HighQualityLightmaps:
			return SupportsMetalMRT();

		case ETargetPlatformFeatures::VirtualTextureStreaming:
			// TODO: should it check r.VirtualTextures for SM5 renderer?
			return bMobileVirtualTextures;

		case ETargetPlatformFeatures::DistanceFieldAO:
			return UsesDistanceFields();

		case ETargetPlatformFeatures::NormalmapLAEncodingMode:
		{
			static IConsoleVariable* CompressorCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("cook.ASTCTextureCompressor"));
			const bool bUsesARMCompressor = (CompressorCVar ? (CompressorCVar->GetInt() != 0) : false);
			return bUsesARMCompressor;
		}

		case ETargetPlatformFeatures::ShowAsPlatformGroup:
			return false;

		case ETargetPlatformFeatures::SupportsMultipleConnectionTypes:
			return true;

		default:
			break;
	}
	
	return TTargetPlatformSettingsBase<FIOSPlatformProperties>::SupportsFeature(Feature);
}

void FIOSTargetPlatformSettings::GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const
{
	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_SIM(TEXT("SF_METAL_SIM"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
	static FName NAME_SF_METAL_TVOS(TEXT("SF_METAL_TVOS"));
	static FName NAME_SF_METAL_MRT_TVOS(TEXT("SF_METAL_MRT_TVOS"));

	if (bIsTVOS)
	{
		if (SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_MRT_TVOS);
		}

		// because we are currently using IOS settings, we will always use metal, even if Metal isn't listed as being supported
		// however, if MetalMRT is specific and Metal is set to false, then we will just use MetalMRT
		if (SupportsMetal() || !SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_TVOS);
		}
	}
	else
	{
		if (SupportsMetal())
		{
			OutFormats.AddUnique(NAME_SF_METAL);

			bool bEnableSimulatorSupport = false;
			GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableSimulatorSupport"), bEnableSimulatorSupport, GEngineIni);
			if (bEnableSimulatorSupport)
			{
				OutFormats.AddUnique(NAME_SF_METAL_SIM);
			}
		}

		if (SupportsMetalMRT())
		{
			OutFormats.AddUnique(NAME_SF_METAL_MRT);
		}
	}
}

void FIOSTargetPlatformSettings::GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const
{
	GetAllPossibleShaderFormats(OutFormats);
}

#if WITH_ENGINE

void FIOSTargetPlatformSettings::GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const
{
	const bool bMobileDeferredShading = (MobileShadingPath == 1);

	if (SupportsMetalMRT() || bMobileDeferredShading || bMobileForwardEnableClusteredReflections)
	{
		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	OutFormats.Add(FName(TEXT("EncodedHDR")));
}

static const FName NameASTC_RGB_HDR(TEXT("ASTC_RGB_HDR"));
static const FName NameBC5(TEXT("BC5"));
static const FName NameASTC_NormalLA(TEXT("ASTC_NormalLA"));

// we remap some of the defaults
static const FName FormatRemap[] =
{
	// original				ASTC
	FName(TEXT("AutoDXT")),	FName(TEXT("ASTC_RGBAuto")),
	FName(TEXT("DXT1")),	FName(TEXT("ASTC_RGB")),
	FName(TEXT("DXT5")),	FName(TEXT("ASTC_RGBA")),
	FName(TEXT("DXT5n")),	FName(TEXT("ASTC_NormalAG")),
	NameBC5,				FName(TEXT("ASTC_NormalRG")),
	FName(TEXT("BC4")),		FName(TEXT("ETC2_R11")),
	FName(TEXT("BC6H")),	NameASTC_RGB_HDR,
	FName(TEXT("BC7")),		FName(TEXT("ASTC_RGBA_HQ"))
};
static const FName NameG8(TEXT("G8"));
static const FName NameRGBA16F(TEXT("RGBA16F"));

const UTextureLODSettings& FIOSTargetPlatformSettings::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}

#endif // WITH_ENGINE

