// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusComputeKernelDataInterface.generated.h"

struct FOptimusConstantIdentifier;

UINTERFACE()
class OPTIMUSCORE_API UOptimusComputeKernelDataInterface :
	public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface that provides a mechanism for compute kernel to setup its kernel data interface
 */
class OPTIMUSCORE_API IOptimusComputeKernelDataInterface
{
public:
	GENERATED_BODY()
	virtual void SetExecutionDomainConstant(const FOptimusConstantIdentifier& InExecutionDomainConstantIdentifier) = 0;
};
