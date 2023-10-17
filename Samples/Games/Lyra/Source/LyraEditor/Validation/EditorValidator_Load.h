// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidator.h"

#include "EditorValidator_Load.generated.h"

class FText;
class UObject;

UCLASS()
class UEditorValidator_Load : public UEditorValidator
{
	GENERATED_BODY()

public:
	UEditorValidator_Load();

	virtual bool IsEnabled() const override;

	static bool GetLoadWarningsAndErrorsForPackage(const FString& PackageName, TArray<FString>& OutWarningsAndErrors);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context);
	
private:
	static TArray<FString> InMemoryReloadLogIgnoreList;
};
