// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "NNEModelData.h"
#include "NNERuntime.h"
#include "NNERuntimeGPU.h"

#include "NNERuntimeIREEGpu.generated.h"

UCLASS()
class UNNERuntimeIREEGpu : public UObject, public INNERuntime, public INNERuntimeGPU
{
	GENERATED_BODY()

public:
	UNNERuntimeIREEGpu() {};
	virtual ~UNNERuntimeIREEGpu() = default;

	//~ Begin INNERuntime Interface
	virtual FString GetRuntimeName() const override;
	virtual bool CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override;
	//~ End INNERuntime Interface

	//~ Begin INNERuntimeGPU Interface
	virtual bool CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TSharedPtr<UE::NNE::IModelGPU> CreateModelGPU(TObjectPtr<UNNEModelData> ModelData) override;
	//~ End INNERuntimeGPU Interface

	virtual bool IsAvailable() const;
	virtual FGuid GetGUID() const;
	virtual int32 GetVersion() const;
};

UCLASS()
class UNNERuntimeIREECuda : public UNNERuntimeIREEGpu
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;

	//~ Begin INNERuntime Interface
	virtual FString GetRuntimeName() const override;
	//~ End INNERuntime Interface

	//~ Begin UNNERuntimeIREEGpu Interface
	virtual bool IsAvailable() const override;
	virtual FGuid GetGUID() const override;
	virtual int32 GetVersion() const override;
	//~ Begin UNNERuntimeIREEGpu Interface
};

UCLASS()
class UNNERuntimeIREEVulkan : public UNNERuntimeIREEGpu
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;

	//~ Begin INNERuntime Interface
	virtual FString GetRuntimeName() const override;
	//~ End INNERuntime Interface

	//~ Begin UNNERuntimeIREEGpu Interface
	virtual bool IsAvailable() const override;
	virtual FGuid GetGUID() const override;
	virtual int32 GetVersion() const override;
	//~ Begin UNNERuntimeIREEGpu Interface
};