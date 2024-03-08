// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSETextRenderer.h"
#include "Materials/MaterialExpressionTextureSample.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTextRenderer"

UDMMaterialStageExpressionTextRenderer::UDMMaterialStageExpressionTextRenderer()
	: UDMMaterialStageExpressionTextureSampleBase(
		LOCTEXT("Text", "Text"),
		UMaterialExpressionTextureSample::StaticClass()
	)
{
	Menus.Add(EDMExpressionMenu::Texture);

	InputConnectors[0] = {1, LOCTEXT("Text", "Text"), EDMValueType::VT_Text};
}

#undef LOCTEXT_NAMESPACE
