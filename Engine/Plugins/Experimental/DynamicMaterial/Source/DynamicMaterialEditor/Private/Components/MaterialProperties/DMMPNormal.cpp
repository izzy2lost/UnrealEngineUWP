// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPNormal.h"

#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Model/DMMaterialBuildState.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Utils/DMMaterialFunctionLibrary.h"

namespace UE::DynamicMaterialEditor::Private
{
	UMaterialFunctionInterface* GetSafeNormalize()
	{
		static UMaterialFunctionInterface* NormalizeBlend = FDMMaterialFunctionLibrary::Get().GetFunction(
			"SafeNormalize",
			TEXT("/Script/Engine.MaterialFunction'/Engine/Functions/Engine_MaterialFunctions02/SafeNormalize.SafeNormalize'")
		);

		return NormalizeBlend;
	}

	UMaterialFunctionInterface* GetNormalMagnitude()
	{
		static UMaterialFunctionInterface* NormalMagnitude = FDMMaterialFunctionLibrary::Get().GetFunction(
			"MF_DM_Normal_Magnitude",
			TEXT("/Script/Engine.MaterialFunction'/DynamicMaterial/MaterialFunctions/MF_DM_Normal_Magnitude.MF_DM_Normal_Magnitude'")
		);

		return NormalMagnitude;
	}
}

UDMMaterialPropertyNormal::UDMMaterialPropertyNormal()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Normal),
		EDMValueType::VT_Float3_XYZ)
{
}

bool UDMMaterialPropertyNormal::IsValidForModel(UDynamicMaterialModelEditorOnlyData& InModelEditorOnlyData) const
{
	if (InModelEditorOnlyData.GetShadingModel() == EDMMaterialShadingModel::Unlit)
	{
		return false;
	}

	switch (InModelEditorOnlyData.GetBlendMode())
	{
		case EBlendMode::BLEND_Opaque:
		case EBlendMode::BLEND_Masked:
			return true;

		default:
			return false;
	}
}

UMaterialExpression* UDMMaterialPropertyNormal::GetDefaultInput(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::OneVector);
}

TEnumAsByte<EMaterialSamplerType> UDMMaterialPropertyNormal::GetTextureSamplerType() const
{
	return EMaterialSamplerType::SAMPLERTYPE_Normal;
}

void UDMMaterialPropertyNormal::AddOutputProcessor(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	Super::AddOutputProcessor(InBuildState);

	FExpressionInput* MaterialPropertyPtr = InBuildState->GetMaterialProperty(MaterialProperty);

	if (!MaterialPropertyPtr)
	{
		return;
	}

	UMaterialExpression* LastPropertyExpression = MaterialPropertyPtr->Expression;

	if (!LastPropertyExpression)
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* MaterialFunctionCall = FDMMaterialFunctionLibrary::Get().MakeExpression(
		InBuildState->GetDynamicMaterial(),
		UE::DynamicMaterialEditor::Private::GetSafeNormalize(),
		UE_DM_NodeComment_Default
	);

	FExpressionInput* FirstInput = MaterialFunctionCall->GetInput(0);
	if (!FirstInput)
	{
		return;
	}

	LastPropertyExpression->ConnectExpression(FirstInput, MaterialPropertyPtr->OutputIndex);
	MaterialFunctionCall->ConnectExpression(MaterialPropertyPtr, 0);

	MaterialPropertyPtr->OutputIndex = 0;
}

void UDMMaterialPropertyNormal::AddAlphaMultiplier(const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	UDMMaterialValueFloat1* AlphaValue = GetTypedComponent<UDMMaterialValueFloat1>(UDynamicMaterialModelEditorOnlyData::AlphaValueName);

	if (!AlphaValue)
	{
		return;
	}

	FExpressionInput* MaterialPropertyPtr = InBuildState->GetMaterialProperty(MaterialProperty);

	if (!MaterialPropertyPtr)
	{
		return;
	}

	UMaterialExpression* LastPropertyExpression = MaterialPropertyPtr->Expression;

	if (!LastPropertyExpression)
	{
		return;
	}

	AlphaValue->GenerateExpression(InBuildState);

	UMaterialExpression* GlobalOpacityExpression = InBuildState->GetLastValueExpression(AlphaValue);

	if (!GlobalOpacityExpression)
	{
		return;
	}

	UMaterialExpressionMaterialFunctionCall* MaterialFunctionCall = FDMMaterialFunctionLibrary::Get().MakeExpression(
		InBuildState->GetDynamicMaterial(),
		UE::DynamicMaterialEditor::Private::GetNormalMagnitude(),
		UE_DM_NodeComment_Default
	);

	FExpressionInput* FirstInput = MaterialFunctionCall->GetInput(0);

	if (!FirstInput)
	{
		return;
	}

	FExpressionInput* SecondInput = MaterialFunctionCall->GetInput(1);

	if (!SecondInput)
	{
		return;
	}

	LastPropertyExpression->ConnectExpression(FirstInput, MaterialPropertyPtr->OutputIndex);
	GlobalOpacityExpression->ConnectExpression(SecondInput, 0);
	MaterialFunctionCall->ConnectExpression(MaterialPropertyPtr, 0);

	MaterialPropertyPtr->OutputIndex = 0;
}
