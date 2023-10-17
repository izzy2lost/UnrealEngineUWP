// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidator.h"

#include "EditorValidator_Blueprints.generated.h"

class FText;
class UObject;

UCLASS()
class UEditorValidator_Blueprints : public UEditorValidator
{
	GENERATED_BODY()

public:
	UEditorValidator_Blueprints();

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context);
};
