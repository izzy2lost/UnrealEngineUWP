// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphSchema_K2.h"

#include "MVVMConversionFunctionGraphSchema.generated.h"

/**
 *
 */
UCLASS()
class UMVVMConversionFunctionGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
};