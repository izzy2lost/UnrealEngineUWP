// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorFloatBase.h"
#include "PropertyAnimatorBounce.generated.h"

/**
 * Applies an additive bounce movement with various options on supported float properties
 */
UCLASS(AutoExpandCategories=("Animator"))
class PROPERTYANIMATOR_API UPropertyAnimatorBounce : public UPropertyAnimatorFloatBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultControllerName = TEXT("Bounce");

	UPropertyAnimatorBounce();

	void SetInvertEffect(bool bInvert);
	bool GetInvertEffect() const
	{
		return bInvertEffect;
	}

protected:
	virtual void OnInvertEffect() {}

	//~ Begin UPropertyAnimatorFloatBase
	virtual float Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const override;
	//~ End UPropertyAnimatorFloatBase

	/** Invert the effect result */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter="SetInvertEffect", Getter="GetInvertEffect", Category="Animator")
	bool bInvertEffect = false;
};