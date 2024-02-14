// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAssetSchema.h"
#include "AnimNextGraph_EventGraphSchema.generated.h"

UCLASS()
class UAnimNextGraph_EventGraphSchema : public UAnimNextRigVMAssetSchema
{
	GENERATED_BODY()

	// URigVMSchema interface
	virtual bool SupportsUnitFunction(URigVMController* InController, const FRigVMFunction* InUnitFunction) const override;
};

