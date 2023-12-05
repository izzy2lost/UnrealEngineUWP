// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "NNEModelData.h"
#include "NNERuntime.h"
#include "NNERuntimeCPU.h"

#include "NNERuntimeIREECpu.generated.h"

UCLASS()
class UNNERuntimeIREECpu : public UObject, public INNERuntime, public INNERuntimeCPU
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;
	static uint32 MemoryAlignment;

	UNNERuntimeIREECpu() {};
	virtual ~UNNERuntimeIREECpu() = default;

	//~ Begin INNERuntime Interface
	virtual FString GetRuntimeName() const override;
	virtual bool CanCreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(FString FileType, TConstArrayView<uint8> FileData, FGuid FileId, const ITargetPlatform* TargetPlatform) override;
	//~ End INNERuntime Interface

	//~ Begin INNERuntimeCPU Interface
	virtual bool CanCreateModelCPU(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TSharedPtr<UE::NNE::IModelCPU> CreateModelCPU(TObjectPtr<UNNEModelData> ModelData) override;
	//~ End INNERuntimeCPU Interface

	static FString GetIntermediateModelDirPath(const FString& PlatformName, const FString& FileIdString);
	static FString GetCookedModelDirPath(const FString& PlatformName);
	static FString GetPackagedModelDirPath(const FString& PlatformName);

	static void GetUpdatedPlatformConfig(const FString& PlatformName, FConfigFile& ConfigFile, FString& ConfigFilePath);
};