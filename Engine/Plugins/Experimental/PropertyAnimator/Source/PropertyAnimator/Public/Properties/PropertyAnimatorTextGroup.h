// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Properties/PropertyAnimatorCoreGroupBase.h"
#include "PropertyAnimatorTextGroup.generated.h"

/** Group that handles text character range properties */
UCLASS(BlueprintType)
class PROPERTYANIMATOR_API UPropertyAnimatorTextGroup : public UPropertyAnimatorCoreGroupBase
{
	GENERATED_BODY()

public:
	UPropertyAnimatorTextGroup()
		: UPropertyAnimatorCoreGroupBase(TEXT("Text"))
	{}

	//~ Begin UPropertyAnimatorCoreGroupBase
	virtual void ManageProperties(const UPropertyAnimatorCoreContext* InContext, TArray<FPropertyAnimatorCoreData>& InOutProperties) override;
	virtual bool IsPropertySupported(const UPropertyAnimatorCoreContext* InContext) const override;
	//~ End UPropertyAnimatorCoreGroupBase

protected:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Interp, Category="Animator", meta=(ClampMin="0", ClampMax="1"))
	float RangeStart = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Interp, Category="Animator", meta=(ClampMin="0", ClampMax="1"))
	float RangeEnd = 100.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Interp, Category="Animator")
	float RangeOffset = 0.f;
};