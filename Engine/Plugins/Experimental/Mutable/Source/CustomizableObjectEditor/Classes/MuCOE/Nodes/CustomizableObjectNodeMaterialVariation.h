// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "CustomizableObjectNodeMaterialVariation.generated.h"

struct FCustomizableObjectMaterialVariation;


UENUM(BlueprintType)
enum class ECustomizableObjectNodeMaterialVariationType : uint8
{
	Tag 		UMETA(DisplayName = "Tag"),
	State 		UMETA(DisplayName = "State"),
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeMaterialVariation : public UCustomizableObjectNodeVariation
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ECustomizableObjectNodeMaterialVariationType Type = ECustomizableObjectNodeMaterialVariationType::Tag;
	
	// Deprecated properties
	UPROPERTY()
	TArray<FCustomizableObjectMaterialVariation> Variations_DEPRECATED;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	
	// UCustomizableObjectNodeVariation interface
	virtual FName GetCategory() const override;

	bool IsInputPinArray() const;
};

