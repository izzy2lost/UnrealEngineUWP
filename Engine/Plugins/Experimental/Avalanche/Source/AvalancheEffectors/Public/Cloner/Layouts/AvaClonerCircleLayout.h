// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaClonerLayoutBase.h"
#include "AvaPropertyChangeDispatcher.h"
#include "AvaClonerCircleLayout.generated.h"

UCLASS(BlueprintType)
class AVALANCHEEFFECTORS_API UAvaClonerCircleLayout : public UAvaClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;
	
public:
	UAvaClonerCircleLayout()
		: UAvaClonerLayoutBase(
			TEXT("Circle")
			, TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerCircle.NS_ClonerCircle'")
		)
	{}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetCount(int32 InCount);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	int32 GetCount() const
	{
		return Count;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetRadius(float InRadius);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	float GetRadius() const
	{
		return Radius;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetAngleStart(float InAngleStart);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	float GetAngleStart() const
	{
		return AngleStart;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetAngleRatio(float InAngleRatio);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	float GetAngleRatio() const
	{
		return AngleRatio;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetOrientMesh(bool bInOrientMesh);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	bool GetOrientMesh() const
	{
		return bOrientMesh;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetPlane(EAvaClonerPlane InPlane);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	EAvaClonerPlane GetPlane() const
	{
		return Plane;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	const FRotator& GetRotation() const
	{
		return Rotation;
	}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Circle")
	void SetScale(const FVector& InScale);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Circle")
	const FVector& GetScale() const
	{
		return Scale;
	}

	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject
	
protected:
	virtual void OnLayoutParametersChanged(UAvaClonerComponent* InComponent) override;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCount", Getter="GetCount", Category="Layout")
	int32 Count = 3 * 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRadius", Getter="GetRadius", Category="Layout")
	float Radius = 200.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAngleStart", Getter="GetAngleStart", Category="Layout")
	float AngleStart = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetAngleRatio", Getter="GetAngleRatio", Category="Layout")
	float AngleRatio = 1.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetOrientMesh", Getter="GetOrientMesh", Category="Layout")
	bool bOrientMesh = true;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetPlane", Getter="GetPlane", Category="Layout")
	EAvaClonerPlane Plane = EAvaClonerPlane::XY;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetRotation", Getter="GetRotation", Category="Layout", meta=(EditCondition="Plane == EAvaClonerPlane::Custom", EditConditionHides))
	FRotator Rotation = FRotator(0.f, 0.f, 0.f);
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetScale", Getter="GetScale", Category="Layout", meta=(ClampMin="0", AllowPreserveRatio, Delta="0.01"))
	FVector Scale = FVector(1.f, 1.f, 1.f);

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaClonerCircleLayout> PropertyChangeDispatcher;
#endif
};
