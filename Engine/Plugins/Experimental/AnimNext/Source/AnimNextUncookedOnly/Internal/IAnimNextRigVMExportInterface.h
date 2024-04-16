// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAnimNextRigVMExportInterface.generated.h"

struct FAnimNextParamType;

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class ANIMNEXTUNCOOKEDONLY_API UAnimNextRigVMExportInterface : public UInterface
{
	GENERATED_BODY()
};

class ANIMNEXTUNCOOKEDONLY_API IAnimNextRigVMExportInterface
{
	GENERATED_BODY()

public:
	// Get the export type
	virtual FAnimNextParamType GetExportType() const = 0;

	// Get the export name (e.g. asset path + entry name)
	virtual FName GetExportName() const = 0;
};