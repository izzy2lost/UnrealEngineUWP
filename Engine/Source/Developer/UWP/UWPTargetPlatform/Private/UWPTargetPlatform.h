/*=============================================================================
	UWPTargetPlatform.h: Declares the FXboxOneTargetPlatform class.
=============================================================================*/

#pragma once

#include "TargetPlatformBase.h"
#include "UWP/UWPProperties.h"

#include "AllowWindowsPlatformTypes.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

/**
 * FUWPTargetPlatform, abstraction for cooking UWP platforms
 */
class FUWPTargetPlatform
	: public TTargetPlatformBase<FUWPPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FUWPTargetPlatform();

	/**
	 * Destructor.
	 */
	virtual ~FUWPTargetPlatform() {}

public:

	//~ Begin ITargetPlatform Interface

	virtual bool AddDevice(const FString& DeviceName, bool bDefault) override { return false; }

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const override;

	virtual ITargetDevicePtr GetDefaultDevice() const override;

	virtual ITargetDevicePtr GetDevice(const FTargetDeviceId& DeviceId) override;

	virtual ECompressionFlags GetBaseCompressionMethod() const override { return ECompressionFlags::COMPRESS_ZLIB; }

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override { return true; }

	virtual bool IsRunningPlatform() const override { return false; }

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override;

#if WITH_ENGINE
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override { return StaticMeshLODSettings; }

	virtual void GetTextureFormats(const UTexture* InTexture, TArray<FName>& OutFormats) const override
	{
		FName TextureFormatName = GetDefaultTextureFormatName(this, InTexture, EngineSettings, false);
		OutFormats.Add(TextureFormatName);
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override { return *TextureLODSettings; }

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual void GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const override
	{
		static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
		static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));

		OutFormats.AddUnique(NAME_PCD3D_SM5);
		OutFormats.AddUnique(NAME_PCD3D_SM4);
	}

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override 
	{
		GetAllPossibleShaderFormats(OutFormats);
	}
	
	virtual void GetAllCachedShaderFormats( TArray<FName>& OutFormats ) const override {}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static FName NAME_OGG(TEXT("OGG"));

		return NAME_OGG;
	}

#endif //WITH_ENGINE

	DECLARE_DERIVED_EVENT(FUWPTargetPlatform, ITargetPlatform::FOnTargetDeviceDiscovered, FOnTargetDeviceDiscovered);
	virtual FOnTargetDeviceDiscovered& OnDeviceDiscovered( ) override
	{
		return DeviceDiscoveredEvent;
	}

	DECLARE_DERIVED_EVENT(FUWPTargetPlatform, ITargetPlatform::FOnTargetDeviceLost, FOnTargetDeviceLost);
	virtual FOnTargetDeviceLost& OnDeviceLost( ) override
	{
		return DeviceLostEvent;
	}

	//~ End ITargetPlatform Interface

private:

	// Holds the local device.
	ITargetDevicePtr LocalDevice;

#if WITH_ENGINE
	// Holds the Engine INI settings (for quick access).
	FConfigFile EngineSettings;

	// Holds a cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif //WITH_ENGINE

private:

	// Holds an event delegate that is executed when a new target device has been discovered.
	FOnTargetDeviceDiscovered DeviceDiscoveredEvent;

	// Holds an event delegate that is executed when a target device has been lost, i.e. disconnected or timed out.
	FOnTargetDeviceLost DeviceLostEvent;
};


#include "HideWindowsPlatformTypes.h"
