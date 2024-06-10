// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSEWorldPositionNoise.h"

#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "CoreGlobals.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DMMaterialBuildUtils.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTextureSample"

UDMMaterialStageExpressionWorldPositionNoise::UDMMaterialStageExpressionWorldPositionNoise()
	: UDMMaterialStageExpression(
		LOCTEXT("UMaterialExpressionVectorNoise", "Noise"),
		UMaterialExpressionVectorNoise::StaticClass()
	)
	, ShaderOffset(EWorldPositionIncludedOffsets::WPT_Default)
	, NoiseFunction(EVectorNoiseFunction::VNF_VectorALU)
	, Quality(1)
	, bTiling(false)
	, TileSize(300)
{
	bInputRequired = true;
	bAllowNestedInputs = true;

	InputConnectors.Add({1, LOCTEXT("Scale", "Scale"), EDMValueType::VT_Float1});

	OutputConnectors.Add({0, LOCTEXT("ColorRGB", "Color (RGB)"), EDMValueType::VT_Float3_RGB});

	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, ShaderOffset));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, NoiseFunction));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, Quality));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, bTiling));
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, TileSize));
}

void UDMMaterialStageExpressionWorldPositionNoise::GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	if (!IsComponentValid() || !IsComponentAdded())
	{
		return;
	}

	check(MaterialExpressionClass.Get());

	if (InBuildState->HasStageSource(this))
	{
		return;
	}

	UMaterialExpressionWorldPosition* WorldPosition = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionWorldPosition>(UE_DM_NodeComment_Default);
	WorldPosition->WorldPositionShaderOffset = ShaderOffset;

	UMaterialExpressionDivide* Divide = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionDivide>(UE_DM_NodeComment_Default);

	WorldPosition->ConnectExpression(&Divide->A, 0);

	UMaterialExpressionVectorNoise* VectorNoise = InBuildState->GetBuildUtils().CreateExpression<UMaterialExpressionVectorNoise>(UE_DM_NodeComment_Default);
	VectorNoise->NoiseFunction = NoiseFunction;
	VectorNoise->Quality = FMath::Clamp(Quality, 1, 4);
	VectorNoise->bTiling = bTiling ? 1 : 0;
	VectorNoise->TileSize = TileSize;

	Divide->ConnectExpression(&VectorNoise->Position, 0);

	InBuildState->AddStageSourceExpressions(this, {WorldPosition, Divide, VectorNoise});
}

void UDMMaterialStageExpressionWorldPositionNoise::SetShaderOffset(EWorldPositionIncludedOffsets InShaderOffset)
{
	if (ShaderOffset == InShaderOffset)
	{
		return;
	}

	ShaderOffset = InShaderOffset;

	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageExpressionWorldPositionNoise::SetNoiseFunction(EVectorNoiseFunction InNoiseFunction)
{
	if (NoiseFunction == InNoiseFunction)
	{
		return;
	}

	NoiseFunction = InNoiseFunction;

	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageExpressionWorldPositionNoise::SetQuality(int32 InQuality)
{
	if (Quality == InQuality)
	{
		return;
	}

	Quality = InQuality;

	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageExpressionWorldPositionNoise::SetTiling(bool bInTiling)
{
	if (bTiling == bInTiling)
	{
		return;
	}

	bTiling = bInTiling;

	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageExpressionWorldPositionNoise::SetTileSize(int32 InTileSize)
{
	if (TileSize == InTileSize)
	{
		return;
	}

	TileSize = InTileSize;

	Update(EDMUpdateType::Structure);
}

void UDMMaterialStageExpressionWorldPositionNoise::AddDefaultInput(int32 InInputIndex) const
{
	Super::AddDefaultInput(InInputIndex);

	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	switch (InInputIndex)
	{
		case 0:
		{
			UDMMaterialStageInputValue* InputValue = Cast<UDMMaterialStageInputValue>(Stage->GetInputs().Last());
			check(InputValue);

			UDMMaterialValueFloat1* Float1Value = Cast<UDMMaterialValueFloat1>(InputValue->GetValue());
			check(Float1Value);

			Float1Value->SetDefaultValue(10.f);
			Float1Value->ApplyDefaultValue();
			break;
		}

		default:
			// Do nothing
			break;
	}
}

UMaterialExpression* UDMMaterialStageExpressionWorldPositionNoise::GetExpressionForInput(const TArray<UMaterialExpression*>& StageSourceExpressions, int32 InputIdx)
{
	if (InputIdx == 1 && StageSourceExpressions.IsValidIndex(1))
	{
		return StageSourceExpressions[1];
	}

	return Super::GetExpressionForInput(StageSourceExpressions, InputIdx);
}

void UDMMaterialStageExpressionWorldPositionNoise::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	static const FName ShaderOffsetName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, ShaderOffset);
	static const FName NoiseFunctionName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, NoiseFunction);
	static const FName QualityName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, Quality);
	static const FName TilingName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, bTiling);
	static const FName TileSizeName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionWorldPositionNoise, TileSize);

	const FName PropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == ShaderOffsetName
		|| PropertyName == NoiseFunctionName
		|| PropertyName == QualityName
		|| PropertyName == TilingName
		|| PropertyName == TileSizeName)
	{
		Update(EDMUpdateType::Structure);
	}
}

#undef LOCTEXT_NAMESPACE
