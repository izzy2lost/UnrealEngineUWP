// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetSchema.h"
#include "AnimNextAnimationGraphSchema.generated.h"

UCLASS()
class UAnimNextAnimationGraphSchema : public UAnimNextRigVMAssetSchema
{
	GENERATED_BODY()

	// URigVMSchema interface
	virtual bool SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const override;
	virtual bool SupportsDispatchFactory(URigVMController* InController, const FRigVMDispatchFactory* InDispatchFactory) const override;
};
