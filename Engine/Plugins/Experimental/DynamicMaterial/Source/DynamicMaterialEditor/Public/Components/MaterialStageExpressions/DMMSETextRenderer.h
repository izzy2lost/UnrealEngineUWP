// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/MaterialStageExpressions/DMMSETextureSampleBase.h"
#include "DMMSETextRenderer.generated.h"

UCLASS(BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageExpressionTextRenderer : public UDMMaterialStageExpressionTextureSampleBase
{
	GENERATED_BODY()

	friend class SDMComponentEdit;

public:
	UDMMaterialStageExpressionTextRenderer();
};
