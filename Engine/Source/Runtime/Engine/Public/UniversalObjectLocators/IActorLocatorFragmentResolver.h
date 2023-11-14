// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Interface.h"
#include "IActorLocatorFragmentResolver.generated.h"

struct FActorLocatorFragment;

UINTERFACE(MinimalAPI)
class UActorLocatorFragmentResolver
	: public UInterface
{
public:
	GENERATED_BODY()
};

class IActorLocatorFragmentResolver
{
public:
	GENERATED_BODY()

	virtual bool ResolveActorLocatorPayload(const FActorLocatorFragment& Payload, UObject*& OutResult) const = 0;
};
