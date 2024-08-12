// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMTrait.h"
#include "RigVMTrait_AnimNextPublicVariables.generated.h"

class UAnimNextRigVMAsset;

// Represents public variables of an asset via a trait 
USTRUCT(BlueprintType)
struct ANIMNEXT_API FRigVMTrait_AnimNextPublicVariables : public FRigVMTrait
{
	GENERATED_BODY()

	// The asset that any programmatic pins will be derived from
	UPROPERTY(meta = (Hidden))
	TObjectPtr<UAnimNextRigVMAsset> Asset = nullptr;

	// Variable names that are exposed
	UPROPERTY(meta = (Hidden))
	TArray<FName> VariableNames;

	// FRigVMTrait interface
#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
	virtual void GetProgrammaticPins(URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, FRigVMPinInfoArray& OutPinArray) const override;
	virtual bool ShouldCreatePinForProperty(const FProperty* InProperty) const override;
#endif
};
