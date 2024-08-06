// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animators/PropertyAnimatorCoreBase.h"
#include "PropertyAnimatorNumericBase.generated.h"

UENUM(BlueprintType)
enum class EPropertyAnimatorCycleMode : uint8
{
	/** Cycle only once then stop */
	DoOnce,
	/** Cycle and repeat once we reached the end */
	Loop,
	/** Cycle and reverse repeat */
	PingPong
};

/**
 * Animate supported numeric properties with various options
 */
UCLASS(MinimalAPI, Abstract, AutoExpandCategories=("Animator"))
class UPropertyAnimatorNumericBase : public UPropertyAnimatorCoreBase
{
	GENERATED_BODY()

	friend class FPropertyAnimatorCoreEditorDetailCustomization;

public:
	PROPERTYANIMATOR_API void SetMagnitude(float InMagnitude);
	float GetMagnitude() const
	{
		return Magnitude;
	}

	PROPERTYANIMATOR_API void SetCycleDuration(float InCycleDuration);
	float GetCycleDuration() const
	{
		return CycleDuration;
	}

	PROPERTYANIMATOR_API void SetCycleMode(EPropertyAnimatorCycleMode InMode);
	EPropertyAnimatorCycleMode GetCycleMode() const
	{
		return CycleMode;
	}

	PROPERTYANIMATOR_API void SetTimeOffset(double InOffset);
	double GetTimeOffset() const
	{
		return TimeOffset;
	}

	PROPERTYANIMATOR_API void SetRandomTimeOffset(bool bInOffset);
	bool GetRandomTimeOffset() const
	{
		return bRandomTimeOffset;
	}

	PROPERTYANIMATOR_API void SetSeed(int32 InSeed);
	int32 GetSeed() const
	{
		return Seed;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	virtual void OnMagnitudeChanged() {}
	virtual void OnCycleDurationChanged() {}
	virtual void OnCycleModeChanged() {}
	virtual void OnTimeOffsetChanged() {}
	virtual void OnSeedChanged() {}

	//~ Begin UPropertyAnimatorCoreBase
	virtual TSubclassOf<UPropertyAnimatorCoreContext> GetPropertyContextClass(const FPropertyAnimatorCoreData& InProperty) override;
	virtual EPropertyAnimatorPropertySupport IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const override;
	virtual void EvaluateProperties(FInstancedPropertyBag& InParameters) override;
	virtual void OnPropertyLinked(UPropertyAnimatorCoreContext* InLinkedProperty, EPropertyAnimatorPropertySupport InSupport) override;
	//~ End UPropertyAnimatorCoreBase

	/** Evaluate and return float value for a property */
	virtual bool EvaluateProperty(const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorCoreContext* InContext, FInstancedPropertyBag& InParameters, FInstancedPropertyBag& OutEvaluationResult) const
	{
		return false;
	}

	/** Magnitude for the effect on all properties */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(ClampMin="0"))
	float Magnitude = 1.f;

	/** Duration of one cycle for the effect = period of the effect */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(ClampMin="0", Units=Seconds))
	float CycleDuration = 1.f;

	/** Cycle mode for the effect */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator")
	EPropertyAnimatorCycleMode CycleMode = EPropertyAnimatorCycleMode::Loop;

	/** Time gap between each cycle */
	UPROPERTY(EditInstanceOnly, Category="Animator", meta=(ClampMin="0", Units=Seconds, EditCondition="CycleMode != EPropertyAnimatorCycleMode::DoOnce", EditConditionHides))
	float CycleGapDuration = 0.f;

	/** Use random time offset to add variation in animation */
	UPROPERTY(EditInstanceOnly, Setter="SetRandomTimeOffset", Getter="GetRandomTimeOffset", Category="Animator", meta=(InlineEditConditionToggle))
	bool bRandomTimeOffset = false;

	/** Seed to generate per property time offset */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(EditCondition="bRandomTimeOffset"))
	int32 Seed = 0;

	/** Time offset accumulated for each property for every round */
	UPROPERTY(EditInstanceOnly, Setter, Getter, Category="Animator", meta=(Units=Seconds))
	double TimeOffset = 0;

private:
	/** Random stream for time offset */
	FRandomStream RandomStream = FRandomStream(Seed);
};