// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageGradient.h"
#include "DMMSGLinear.generated.h"

struct FDMMaterialBuildState;
class UMaterialExpression;

UENUM(BlueprintType)
enum class ELinearGradientTileType : uint8
{
	NoTile,
	Tile,
	TileAndMirror
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = "Material Designer")
class UDMMaterialStageGradientLinear : public UDMMaterialStageGradient
{
	GENERATED_BODY()

public:
	UDMMaterialStageGradientLinear();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual ELinearGradientTileType GetTilingType() const { return Tiling; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	DYNAMICMATERIALEDITOR_API virtual void SetTilingType(ELinearGradientTileType InType);

	//~ Begin UDMMaterialStageSource
	DYNAMICMATERIALEDITOR_API virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	//~ End UDMMaterialStageSource

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetTilingType, Setter=SetTilingType, BlueprintSetter = SetTilingType, 
		Category = "Material Designer")
	ELinearGradientTileType Tiling;
};
