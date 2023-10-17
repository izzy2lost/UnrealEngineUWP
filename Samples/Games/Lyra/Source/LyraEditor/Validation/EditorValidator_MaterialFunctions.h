// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidator.h"

#include "EditorValidator_MaterialFunctions.generated.h"

class FText;
class UObject;

UCLASS()
class UEditorValidator_MaterialFunctions : public UEditorValidator
{
	GENERATED_BODY()

public:
	UEditorValidator_MaterialFunctions();

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context);
};
