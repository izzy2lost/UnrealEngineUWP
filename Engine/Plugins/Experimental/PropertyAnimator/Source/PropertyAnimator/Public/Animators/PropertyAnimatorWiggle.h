// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorFloatBase.h"
#include "PropertyAnimatorWiggle.generated.h"

/**
 * Applies an additive random wiggle movement with various options on supported float properties
 */
UCLASS(AutoExpandCategories=("Animator"))
class PROPERTYANIMATOR_API UPropertyAnimatorWiggle : public UPropertyAnimatorFloatBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultControllerName = TEXT("Wiggle");

	UPropertyAnimatorWiggle();

protected:
	//~ Begin UPropertyAnimatorFloatBase
	virtual float Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const override;
	//~ End UPropertyAnimatorFloatBase
};