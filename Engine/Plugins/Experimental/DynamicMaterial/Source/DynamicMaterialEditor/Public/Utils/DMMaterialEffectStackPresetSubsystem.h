// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "DMMaterialEffectStackPresetSubsystem.generated.h"

class UDMMaterialEffect;
class UDMMaterialEffectStack;
struct FDMMaterialEffectStackJson;

UCLASS(BlueprintType)
class UDMMaterialEffectStackPresetSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	static UDMMaterialEffectStackPresetSubsystem* Get();

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool SavePreset(const FString& InPresetName, const FDMMaterialEffectStackJson& InPreset) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool LoadPreset(const FString& InPresetName, FDMMaterialEffectStackJson& OutPreset) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	bool RemovePreset(const FString& InPresetName) const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	TArray<FString> GetPresetNames() const;
};
