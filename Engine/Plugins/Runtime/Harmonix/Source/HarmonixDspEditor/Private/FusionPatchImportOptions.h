// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Engine/DeveloperSettings.h"

#include "Sound/SoundWave.h"
#include "Sound/SoundWaveLoadingBehavior.h"

#include "FusionPatchImportOptions.generated.h"

UCLASS(config = Editor)
class UFusionPatchImportOptions : public UObject
{
	GENERATED_BODY()

public:

	/** The directory to save samples to */
	UPROPERTY(config, EditAnywhere, Category = "Import Options", Meta = (DisplayName = "Sound Waves Import Folder", ContentDir))
	FDirectoryPath SamplesImportDir;

	/** The loading behavior to apply to the imported samples */
	UPROPERTY(EditAnywhere, Category = "Import Options", Meta = (DisplayName = "Sound Wave Loading Behavior"))
	ESoundWaveLoadingBehavior SampleLoadingBehavior = ESoundWaveLoadingBehavior::RetainOnLoad;

	/** The compression type to apply to the imported samples */
	UPROPERTY(EditAnywhere, Category = "Import Options", Meta = (DisplayName = "Sound Wave Compression Type"))
	ESoundAssetCompressionType SampleCompressionType = ESoundAssetCompressionType::BinkAudio;

#if WITH_EDITOR
	/** UObject interface */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override
	{
		SaveConfig();
	}
#endif
};