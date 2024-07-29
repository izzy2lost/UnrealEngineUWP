// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "NNEModelData.h"

#include "NNEDenoiserAsset.generated.h"

/** Tiling configuration for fixed and dynamic size models */
USTRUCT(BlueprintType)
struct FTilingConfig
{
	GENERATED_BODY()

	/** Tile size alignment (applies only to dynamic size models) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser, meta = (DisplayName = "Size Alignment"))
	int32 Alignment = 1;

	/** Tile overlap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 Overlap = 0;

	/** Maximum tile size (applies only to dynamic size models) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 MaxSize = 0;

	/** Minimum tile size (applies only to dynamic size models) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	int32 MinSize = 1;
};

/** Denoiser model data asset */
UCLASS(BlueprintType)
class NNEDENOISER_API UNNEDenoiserAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** NNE model data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	TSoftObjectPtr<UNNEModelData> ModelData;

	/** Input mapping table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser, meta = (RequiredAssetDataTags = "RowStructure=/Script/NNEDenoiser.NNEDenoiserModelIOMappingData"))
	TSoftObjectPtr<UDataTable> InputMapping;

	/** Output mapping table */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser, meta = (RequiredAssetDataTags = "RowStructure=/Script/NNEDenoiser.NNEDenoiserModelIOMappingData"))
	TSoftObjectPtr<UDataTable> OutputMapping;

	/** Tiling configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NNEDenoiser)
	FTilingConfig TilingConfig{};
};