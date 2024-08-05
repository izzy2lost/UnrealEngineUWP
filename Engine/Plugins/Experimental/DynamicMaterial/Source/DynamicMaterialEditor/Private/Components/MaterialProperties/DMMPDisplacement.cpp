// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialProperties/DMMPDisplacement.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"

UDMMaterialPropertyDisplacement::UDMMaterialPropertyDisplacement()
	: UDMMaterialProperty(
		EDMMaterialPropertyType(EDMMaterialPropertyType::Displacement),
		EDMValueType::VT_Float3_XYZ)
{
}

UMaterialExpression* UDMMaterialPropertyDisplacement::GetDefaultInput(
	const TSharedRef<FDMMaterialBuildState>& InBuildState) const
{
	return CreateConstant(InBuildState, FVector::ZeroVector);
}
