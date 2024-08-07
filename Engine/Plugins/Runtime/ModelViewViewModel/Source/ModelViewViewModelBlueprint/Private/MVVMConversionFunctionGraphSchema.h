// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphSchema_K2.h"

#include "MVVMConversionFunctionGraphSchema.generated.h"

/**
 * Schema for conversion functions, adds pin metadata needed on connections for MVVM
 */
UCLASS()
class UMVVMConversionFunctionGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
};