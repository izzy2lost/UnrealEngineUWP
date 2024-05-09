// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerExtensionBase.h"
#include "CEPropertyChangeDispatcher.h"
#include "CEClonerRangeExtension.generated.h"

/** Extension dealing with range options */
UCLASS(MinimalAPI, BlueprintType, Within=CEClonerComponent)
class UCEClonerRangeExtension : public UCEClonerExtensionBase
{
	GENERATED_BODY()

public:
	UCEClonerRangeExtension()
		: UCEClonerExtensionBase(
			TEXT("Range")
			, 0
#if WITH_EDITOR
			, TEXT("Range")
			, 5
#endif
		)
	{}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeEnabled(bool bInRangeEnabled);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetRangeEnabled() const
	{
		return bRangeEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeOffsetMin(const FVector& InRangeOffsetMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeOffsetMin() const
	{
		return RangeOffsetMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeOffsetMax(const FVector& InRangeOffsetMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeOffsetMax() const
	{
		return RangeOffsetMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeRotationMin(const FRotator& InRangeRotationMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FRotator& GetRangeRotationMin() const
	{
		return RangeRotationMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeRotationMax(const FRotator& InRangeRotationMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FRotator& GetRangeRotationMax() const
	{
		return RangeRotationMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeScaleUniform(bool bInRangeScaleUniform);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetRangeScaleUniform() const
	{
		return bRangeScaleUniform;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeScaleMin(const FVector& InRangeScaleMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeScaleMin() const
	{
		return RangeScaleMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeScaleMax(const FVector& InRangeScaleMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	const FVector& GetRangeScaleMax() const
	{
		return RangeScaleMax;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeScaleUniformMin(float InRangeScaleUniformMin);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetRangeScaleUniformMin() const
	{
		return RangeScaleUniformMin;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetRangeScaleUniformMax(float InRangeScaleUniformMax);

	UFUNCTION(BlueprintPure, Category="Cloner")
	float GetRangeScaleUniformMax() const
	{
		return RangeScaleUniformMax;
	}

protected:
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

	//~ Begin UCEClonerExtensionBase
	virtual void OnExtensionParametersChanged(UCEClonerComponent* InComponent) override;
	//~ End UCEClonerExtensionBase

	/** Use random range transforms for each clones */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeEnabled", Getter="GetRangeEnabled", DisplayName="Enabled", Category="Range")
	bool bRangeEnabled = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeOffsetMin", Getter="GetRangeOffsetMin", DisplayName="OffsetMin", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FVector RangeOffsetMin = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeOffsetMax", Getter="GetRangeOffsetMax", DisplayName="OffsetMax", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FVector RangeOffsetMax = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeRotationMin", Getter="GetRangeRotationMin", DisplayName="RotationMin", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FRotator RangeRotationMin = FRotator::ZeroRotator;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeRotationMax", Getter="GetRangeRotationMax", DisplayName="RotationMax", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	FRotator RangeRotationMax = FRotator::ZeroRotator;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeScaleUniform", Getter="GetRangeScaleUniform", DisplayName="ScaleUniformEnabled", Category="Range", meta=(EditCondition="bRangeEnabled", EditConditionHides))
	bool bRangeScaleUniform = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeScaleMin", Getter="GetRangeScaleMin", DisplayName="ScaleMin", Category="Range", meta=(AllowPreserveRatio, Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && !bRangeScaleUniform", EditConditionHides))
	FVector RangeScaleMin = FVector::OneVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeScaleMax", Getter="GetRangeScaleMax", DisplayName="ScaleMax", Category="Range", meta=(AllowPreserveRatio, Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && !bRangeScaleUniform", EditConditionHides))
	FVector RangeScaleMax = FVector::OneVector;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeScaleUniformMin", Getter="GetRangeScaleUniformMin", DisplayName="ScaleMin", Category="Range", meta=(Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && bRangeScaleUniform", EditConditionHides))
	float RangeScaleUniformMin = 1.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRangeScaleUniformMax", Getter="GetRangeScaleUniformMax", DisplayName="ScaleMax", Category="Range", meta=(Delta="0.0001", ClampMin="0", EditCondition="bRangeEnabled && bRangeScaleUniform", EditConditionHides))
	float RangeScaleUniformMax = 1.f;

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TCEPropertyChangeDispatcher<UCEClonerRangeExtension> PropertyChangeDispatcher;
#endif
};
