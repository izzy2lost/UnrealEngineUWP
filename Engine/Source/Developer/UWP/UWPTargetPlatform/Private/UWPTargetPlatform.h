/*=============================================================================
	UWPTargetPlatform.h: Declares the FXboxOneTargetPlatform class.
=============================================================================*/

#pragma once

#include "Common/TargetPlatformBase.h"
#include "Runtime/Core/Public/UWP/UWPPlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "UWPTargetDevice.h"
#include "Misc/ScopeLock.h"
#include "IUWPDeviceDetectorModule.h"

#include "Windows/AllowWindowsPlatformTypes.h"

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "UWPTargetPlatform"

/**
 * FUWPTargetPlatform, abstraction for cooking UWP platforms
 */
class UWPTARGETPLATFORM_API FUWPTargetPlatform
	: public TTargetPlatformBase<FUWPPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FUWPTargetPlatform(const FName& InPlatformName);

	/**
	 * Destructor.
	 */
	virtual ~FUWPTargetPlatform();

public:

	//~ Begin ITargetPlatform Interface

	virtual bool AddDevice(const FString& DeviceName, bool bDefault) override { return false; }

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const override;

	virtual ITargetDevicePtr GetDefaultDevice() const override;

	virtual ITargetDevicePtr GetDevice(const FTargetDeviceId& DeviceId) override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override { return true; }

	virtual bool IsRunningPlatform() const override { return false; }

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override;

#if WITH_ENGINE

	virtual const struct FPlatformAudioCookOverrides* GetAudioCompressionSettings() const ;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override { return StaticMeshLODSettings; }

	virtual void GetTextureFormats(const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override { return *TextureLODSettings; }

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual void GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const override;

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override;
	
	//virtual void GetAllCachedShaderFormats( TArray<FName>& OutFormats ) const override {}

	virtual FName GetWaveFormat( const class USoundWave* Wave ) const override
	{
		static FName NAME_OGG(TEXT("OGG"));

		return NAME_OGG;
	}

	virtual void GetAllWaveFormats(TArray<FName>& OutFormats) const override
	{
		static FName NAME_OGG(TEXT("OGG"));
		static FName NAME_OPUS(TEXT("OPUS"));
		OutFormats.Add(NAME_OGG);
		OutFormats.Add(NAME_OPUS);
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

	//virtual bool RequiresUserCredentials() const override
	//{
	//	return true;
	//}

	virtual bool SupportsVariants() const override
	{
		return true;
	}

	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("UWPVariantTitle", "Build Type");
	}

	//~ End ITargetPlatform Interface

protected:

	virtual bool SupportsDevice(FName DeviceType, bool DeviceIs64Bits) = 0;

private:

	void OnDeviceDetected(const FUWPDeviceInfo& Info);

	mutable FCriticalSection DevicesLock;
	TArray<ITargetDevicePtr> Devices;

	FDelegateHandle DeviceDetectedRegistration;

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

	IUWPDeviceDetectorModule& UWPDeviceDetectorModule;
};

template <bool Is64Bit>
class TUWPTargetPlatform : public FUWPTargetPlatform
{
public:
	TUWPTargetPlatform()
		: FUWPTargetPlatform(Is64Bit ? FName("UWP64") : FName("UWP32"))
	{

	}

	virtual FText GetVariantTitle() const override
	{
		return LOCTEXT("UWPVariantTitle", "Build Type");
	}

	virtual FString PlatformName() const override
	{
		return Is64Bit ? TEXT("UWP64") : TEXT("UWP32");
	}

	virtual FText GetVariantDisplayName() const override
	{
		return Is64Bit ? LOCTEXT("UWP64VariantDisplayName", "UWP (64 bit)") : LOCTEXT("UWP32VariantDisplayName", "UWP (32 bit)");
	}

	virtual float GetVariantPriority() const override
	{
		return Is64Bit ? 1.0f : 0.0f;
	}

protected:
	virtual bool SupportsDevice(FName DeviceType, bool DeviceIs64Bits)
	{
		if (DeviceIs64Bits)
		{
			return Is64Bit || DeviceType != UWPDeviceTypes::Xbox;
		}
		else
		{
			return !Is64Bit;
		}
	}
};

#undef LOCTEXT_NAMESPACE

#include "Windows/HideWindowsPlatformTypes.h"
