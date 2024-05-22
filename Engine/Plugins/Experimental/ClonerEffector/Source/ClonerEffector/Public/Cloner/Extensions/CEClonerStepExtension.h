// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CEClonerExtensionBase.h"
#include "CEPropertyChangeDispatcher.h"
#include "CEClonerStepExtension.generated.h"

/** Extension dealing with delta step accumulated options */
UCLASS(MinimalAPI, BlueprintType, Within=CEClonerComponent)
class UCEClonerStepExtension : public UCEClonerExtensionBase
{
	GENERATED_BODY()

public:
	UCEClonerStepExtension()
		: UCEClonerExtensionBase(
			TEXT("Step")
			, 0
#if WITH_EDITOR
			, UE::ClonerEffector::ClonerSection::ClonerSection
#endif
		)
	{}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetDeltaStepEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category="Cloner")
	bool GetDeltaStepEnabled() const
	{
		return bDeltaStepEnabled;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetDeltaStepPosition(const FVector& InPosition);

	UFUNCTION(BlueprintPure, Category="Cloner")
	FVector GetDeltaStepPosition() const
	{
		return DeltaStepPosition;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetDeltaStepRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintPure, Category="Cloner")
	FRotator GetDeltaStepRotation() const
	{
		return DeltaStepRotation;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner")
	CLONEREFFECTOR_API void SetDeltaStepScale(const FVector& InScale);

	UFUNCTION(BlueprintPure, Category="Cloner")
	FVector GetDeltaStepScale() const
	{
		return DeltaStepScale;
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

	/** Enable steps to add delta variation on each clone instance */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetDeltaStepEnabled", Getter="GetDeltaStepEnabled", DisplayName="Enabled", Category="Step")
	bool bDeltaStepEnabled = false;

	/** Amount of position difference between one step and the next one */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetDeltaStepPosition", Getter="GetDeltaStepPosition", DisplayName="Position", Category="Step", meta=(EditCondition="bDeltaStepEnabled", EditConditionHides))
	FVector DeltaStepPosition = FVector(0.f);

	/** Amount of rotation difference between one step and the next one */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetDeltaStepRotation", Getter="GetDeltaStepRotation", DisplayName="Rotation", Category="Step", meta=(EditCondition="bDeltaStepEnabled", EditConditionHides))
	FRotator DeltaStepRotation = FRotator(0.f);

	/** Amount of scale difference between one step and the next one */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetDeltaStepScale", Getter="GetDeltaStepScale", DisplayName="Scale", Category="Step", meta=(MotionDesignVectorWidget, AllowPreserveRatio="XYZ", Delta="0.0001", EditCondition="bDeltaStepEnabled", EditConditionHides))
	FVector DeltaStepScale = FVector(0.f);

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TCEPropertyChangeDispatcher<UCEClonerStepExtension> PropertyChangeDispatcher;
#endif
};