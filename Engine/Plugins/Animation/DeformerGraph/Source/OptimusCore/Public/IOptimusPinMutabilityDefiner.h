// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "IOptimusPinMutabilityDefiner.generated.h"


class UOptimusNodePin;
UINTERFACE()
class OPTIMUSCORE_API UOptimusPinMutabilityDefiner :
	public UInterface
{
	GENERATED_BODY()
};


class IOptimusPinMutabilityDefiner
{
	GENERATED_BODY()

public:
	/** Returns the component binding for the node that this interface is implemented on */
	virtual bool IsOutputPinMutable(const UOptimusNodePin* InPin) const = 0;
};
