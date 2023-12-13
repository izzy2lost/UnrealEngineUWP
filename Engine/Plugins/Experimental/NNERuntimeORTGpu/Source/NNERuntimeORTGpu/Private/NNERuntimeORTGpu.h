// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNERuntime.h"
#include "NNERuntimeGPU.h"
#include "Templates/UniquePtr.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/UObjectBaseUtility.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include "core/session/onnxruntime_cxx_api.h"
NNE_THIRD_PARTY_INCLUDES_END

#include "NNERuntimeORTGpu.generated.h"

UENUM()
enum class ENNERuntimeORTGpuProvider : uint8
{
	None,
	Dml,
	Cuda
};

UCLASS()
class UNNERuntimeORTGpuImpl : public UObject, public INNERuntime, public INNERuntimeGPU
{
	GENERATED_BODY()

public:
	static FGuid GUID;
	static int32 Version;
	ENNERuntimeORTGpuProvider Provider;

	TUniquePtr<Ort::Env> ORTEnvironment;
	UNNERuntimeORTGpuImpl() {};
	virtual ~UNNERuntimeORTGpuImpl() {}

	void Init(ENNERuntimeORTGpuProvider Provider);

	virtual FString GetRuntimeName() const override;

	virtual bool CanCreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override;
	virtual TSharedPtr<UE::NNE::FSharedModelData> CreateModelData(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) override;
	virtual FString GetModelDataIdentifier(const FString& FileType, TConstArrayView<uint8> FileData, const TMap<FString, TConstArrayView<uint8>>& AdditionalFileData, const FGuid& FileId, const ITargetPlatform* TargetPlatform) const override;

	virtual bool CanCreateModelGPU(TObjectPtr<UNNEModelData> ModelData) const override;
	virtual TSharedPtr<UE::NNE::IModelGPU> CreateModelGPU(TObjectPtr<UNNEModelData> ModelData) override;
};