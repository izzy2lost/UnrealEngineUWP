// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextParamInstanceIdentifier.h"
#include "ParamUtils.h"
#include "UniversalObjectLocator.h"
#include "UniversalObjectLocatorStringParams.h"
#include "AnimNextParamUniversalObjectLocator.generated.h"

// TODO: Move this to UncookedOnly module once schedules are refactored - this should be an editor-only type
// Wraps a universal object locator to identify external object instances to the parameter system
USTRUCT()
struct FAnimNextParamUniversalObjectLocator : public FAnimNextParamInstanceIdentifier
{
	GENERATED_BODY()

	// FAnimNextParamInstanceIdentifier interface
	virtual bool IsValid() const override
	{
		return !Locator.IsEmpty();
	}

	virtual FName ToName() const override
	{
		return UE::AnimNext::FParamUtils::LocatorToName(Locator);
	}
	
	virtual void FromName(FName InName) override
	{
		Locator = FUniversalObjectLocator::FromString(InName.ToString(), UE::UniversalObjectLocator::FParseStringParams());
	}

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (LocatorContext="AnimNextContext"))
	FUniversalObjectLocator Locator;
};
