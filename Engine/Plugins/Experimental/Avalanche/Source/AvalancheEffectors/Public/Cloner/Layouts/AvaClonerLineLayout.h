// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaClonerLayoutBase.h"
#include "AvaPropertyChangeDispatcher.h"
#include "AvaClonerLineLayout.generated.h"

UCLASS(BlueprintType)
class AVALANCHEEFFECTORS_API UAvaClonerLineLayout : public UAvaClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;
	
public:
	UAvaClonerLineLayout()
		: UAvaClonerLayoutBase(
			TEXT("Line")
			, TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerLine.NS_ClonerLine'")
		)
	{}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Line")
	void SetCount(int32 InCount);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Line")
	int32 GetCount() const
	{
		return Count;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Line")
	void SetSpacing(float InSpacing);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Line")
	float GetSpacing() const
	{
		return Spacing;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Line")
	void SetAxis(EAvaClonerAxis InAxis);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Line")
	EAvaClonerAxis GetAxis() const
	{
		return Axis;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Line")
	void SetDirection(const FVector& InDirection);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Line")
	const FVector& GetDirection() const
	{
		return Direction;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Line")
	void SetRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Line")
	const FRotator& GetRotation() const
	{
		return Rotation;
	}
	
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

protected:
	virtual void OnLayoutParametersChanged(UAvaClonerComponent* InComponent) override;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCount", Getter="GetCount", Category="Layout")
    int32 Count = 10;
    	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacing", Getter="GetSpacing", Category="Layout")
	float Spacing = 105.f;
    	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAxis", Getter="GetAxis", Category="Layout")
	EAvaClonerAxis Axis = EAvaClonerAxis::Y;
    
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetDirection", Getter="GetDirection", Category="Layout", meta=(ClampMin="0", ClampMax="1", EditCondition="Axis == EAvaClonerAxis::Custom", EditConditionHides))
	FVector Direction = FVector::YAxisVector;
    
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRotation", Getter="GetRotation", Category="Layout")
	FRotator Rotation = FRotator(0.f);
	
private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaClonerLineLayout> PropertyChangeDispatcher;
#endif
};