// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/PropertyGenerators/DMComponentPropertyRowGenerator.h"

class IPropertyHandle;
struct FFunctionExpressionInput;

class FDMMaterialStageFunctionPropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMMaterialStageFunctionPropertyRowGenerator>& Get();

	virtual ~FDMMaterialStageFunctionPropertyRowGenerator() override = default;

	static void ApplyMetaData(const FFunctionExpressionInput& InFunctionInput, const TSharedRef<IPropertyHandle>& InPropertyHandle);

	//~ Begin FDMComponentPropertyRowGenerator
	virtual void AddComponentProperties(const TSharedRef<SDMMaterialComponentEditor>& InComponentEditorWidget, UDMMaterialComponent* InComponent,
		TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects) override;
	//~ End FDMComponentPropertyRowGenerator
};
