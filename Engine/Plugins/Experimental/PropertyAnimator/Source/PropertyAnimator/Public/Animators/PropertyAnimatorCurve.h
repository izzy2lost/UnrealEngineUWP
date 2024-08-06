// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorNumericBase.h"
#include "PropertyAnimatorCurve.generated.h"

class UPropertyAnimatorEaseCurve;
class UPropertyAnimatorWaveCurve;

USTRUCT(BlueprintType)
struct FPropertyAnimatorCurveEasing
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, Category="Animator")
	TObjectPtr<UPropertyAnimatorEaseCurve> EaseCurve;

	UPROPERTY(EditInstanceOnly, Interp, Category="Animator", meta=(ClampMin="0", Units=Seconds))
	float EaseDuration = 0.f;
};

/**
 * Applies a wave movement from a curve on supported float properties
 */
UCLASS(MinimalAPI, AutoExpandCategories=("Animator"))
class UPropertyAnimatorCurve : public UPropertyAnimatorNumericBase
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* DefaultAnimatorName = TEXT("Curve");

	UPropertyAnimatorCurve();

	void SetWaveCurve(UPropertyAnimatorWaveCurve* InCurve);

	UPropertyAnimatorWaveCurve* GetWaveCurve() const
	{
		return WaveCurve;
	}

	void SetEaseInEnabled(bool bInEnabled);

	bool GetEaseInEnabled() const
	{
		return bEaseInEnabled;
	}

	void SetEaseIn(const FPropertyAnimatorCurveEasing& InEasing);

	const FPropertyAnimatorCurveEasing& GetEaseIn() const
	{
		return EaseIn;
	}

	void SetEaseOutEnabled(bool bInEnabled);

	bool GetEaseOutEnabled() const
	{
		return bEaseOutEnabled;
	}

	void SetEaseOut(const FPropertyAnimatorCurveEasing& InEasing);

	const FPropertyAnimatorCurveEasing& GetEaseOut() const
	{
		return EaseOut;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UPropertyAnimatorFloatBase
	virtual bool EvaluateProperty(const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorCoreContext* InContext, FInstancedPropertyBag& InParameters, FInstancedPropertyBag& OutEvaluationResult) const override;
	//~ End UPropertyAnimatorFloatBase

	void OnEaseInChanged();
	void OnEaseOutChanged();
	virtual void OnCycleDurationChanged() override;

	/** The wave curve to sample for the animation */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator")
	TObjectPtr<UPropertyAnimatorWaveCurve> WaveCurve;

	UPROPERTY(EditInstanceOnly, Setter="SetEaseInEnabled", Getter="GetEaseInEnabled", Category="Animator", meta=(InlineEditConditionToggle))
	bool bEaseInEnabled = false;

	/** Ease in for this effect */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(EditCondition="bEaseInEnabled"))
	FPropertyAnimatorCurveEasing EaseIn;

	UPROPERTY(EditInstanceOnly, Setter="SetEaseOutEnabled", Getter="GetEaseOutEnabled", Category="Animator", meta=(InlineEditConditionToggle))
	bool bEaseOutEnabled = false;

	/** Ease out for this effect */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(EditCondition="bEaseOutEnabled"))
    FPropertyAnimatorCurveEasing EaseOut;
};