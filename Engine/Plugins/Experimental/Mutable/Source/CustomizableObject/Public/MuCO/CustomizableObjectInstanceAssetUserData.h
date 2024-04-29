// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"
#include "GameplayTagContainer.h"

#include "CustomizableObjectInstanceAssetUserData.generated.h"

class UAnimInstance;


USTRUCT(BlueprintType)
struct FCustomizableObjectAnimationSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = CustomizableObjectInstance)
	FName Name;

	UPROPERTY(BlueprintReadWrite, Category = CustomizableObjectInstance)
	TSoftClassPtr<UAnimInstance> AnimInstance;
};


/** Additional data attached to Skeletal Meshes. */
UCLASS(BlueprintType)
class CUSTOMIZABLEOBJECT_API UCustomizableObjectInstanceUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = CustomizableObjectInstance)
	FGameplayTagContainer AnimationGameplayTag;

	UPROPERTY(BlueprintReadWrite, Category = CustomizableObjectInstance)
	TArray<FCustomizableObjectAnimationSlot> AnimationSlots;
};
