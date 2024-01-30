// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Color.h"
#include "UObject/ObjectPtr.h"
#include "AvaModifiersParametricMaterial.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UObject;

/** Use this if you want a parametric material */
USTRUCT(BlueprintType)
struct AVALANCHEMODIFIERS_API FAvaModifiersParametricMaterial
{
	GENERATED_BODY()
	
	FAvaModifiersParametricMaterial();

	FAvaModifiersParametricMaterial(const FAvaModifiersParametricMaterial& Other);
	FAvaModifiersParametricMaterial& operator=(const FAvaModifiersParametricMaterial& Other);

	UPROPERTY()
	FLinearColor MaskColor;

	UMaterial* GetDefaultMaterial() const;
	UMaterialInstanceDynamic* GetMaterial() const;
	void ApplyChanges(UObject* Outer = nullptr);

protected:
	UPROPERTY()
	TObjectPtr<UMaterial> DefaultMaterial;

	UPROPERTY(Transient, Instanced)
	TObjectPtr<UMaterialInstanceDynamic> InstanceMaterial;

	void ApplyParams() const;
	void CreateAndApply(UObject* Outer = nullptr);
	void EnsureCurrentMaterial(UObject* Outer = nullptr);
	UMaterial* LoadResource() const;
};