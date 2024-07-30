// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "NNEDenoiserResourceName.h"

#include "NNEDenoiserIOMappingData.generated.h"

/** An enum to represent resource names used for denoiser input mapping */
UENUM()
enum EInputResourceName : uint8
{
	I_Color UMETA(DisplayName="Color"),
	I_Albedo UMETA(DisplayName="Albedo"),
	I_Normal UMETA(DisplayName="Normal"),
	I_Output UMETA(DisplayName="Output")
};

/** An enum to represent resource names used for denoiser output mapping */
UENUM()
enum EOutputResourceName : uint8
{
	O_Output UMETA(DisplayName="Output")
};

/** An enum to represent resource names used for temporal denoiser input mapping */
UENUM()
enum ETemporalInputResourceName : uint8
{
	TI_Color UMETA(DisplayName="Color"),
	TI_Albedo UMETA(DisplayName="Albedo"),
	TI_Normal UMETA(DisplayName="Normal"),
	TI_Flow UMETA(DisplayName="Flow"),
	TI_Output UMETA(DisplayName="Output")
};

/** An enum to represent resource names used for temporal denoiser output mapping */
UENUM()
enum ETemporalOutputResourceName : uint8
{
	TO_Output UMETA(DisplayName="Output")
};

/** Table row base for denoiser basic input and output mapping */
USTRUCT(BlueprintType)
struct FNNEDenoiserBaseMappingData : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:
	/** Input/output tensor index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 TensorIndex = 0;

	/** Input/output tensor channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 TensorChannel = 0;

	/** Resource channel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 ResourceChannel = 0;
};

/** Table row base for denoiser input mapping */
USTRUCT(BlueprintType)
struct FNNEDenoiserInputMappingData : public FNNEDenoiserBaseMappingData
{
	GENERATED_USTRUCT_BODY()

	/** Mapped resource name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TEnumAsByte<EInputResourceName> Resource = EInputResourceName::I_Color;
};

/** Table row base for denoiser output mapping */
USTRUCT(BlueprintType)
struct FNNEDenoiserOutputMappingData : public FNNEDenoiserBaseMappingData
{
	GENERATED_USTRUCT_BODY()

	/** Mapped resource name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TEnumAsByte<EOutputResourceName> Resource = EOutputResourceName::O_Output;
};

/** Table row base for temporal denoiser input mapping */
USTRUCT(BlueprintType)
struct FNNEDenoiserTemporalInputMappingData : public FNNEDenoiserBaseMappingData
{
	GENERATED_USTRUCT_BODY()

public:
	/** Mapped resource name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TEnumAsByte<ETemporalInputResourceName> Resource = ETemporalInputResourceName::TI_Color;

	/** Resource frame index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 FrameIndex = 0;
};

/** Table row base for temporal denoiser output mapping */
USTRUCT(BlueprintType)
struct FNNEDenoiserTemporalOutputMappingData : public FNNEDenoiserBaseMappingData
{
	GENERATED_USTRUCT_BODY()

	/** Mapped resource name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TEnumAsByte<ETemporalOutputResourceName> Resource = ETemporalOutputResourceName::TO_Output;
};

namespace UE::NNEDenoiser
{

EResourceName ToResourceName(EInputResourceName Name);
EResourceName ToResourceName(EOutputResourceName Name);
EResourceName ToResourceName(ETemporalInputResourceName Name);
EResourceName ToResourceName(ETemporalOutputResourceName Name);

} // namespace UE::NNEDenoiser