// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaClonerLayoutBase.h"
#include "AvaPropertyChangeDispatcher.h"
#include "AvaClonerCylinderLayout.generated.h"

UCLASS(BlueprintType)
class AVALANCHEEFFECTORS_API UAvaClonerCylinderLayout : public UAvaClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;
	
public:
	UAvaClonerCylinderLayout()
		: UAvaClonerLayoutBase(
			TEXT("Cylinder")
			, TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerCylinder.NS_ClonerCylinder'")
		)
	{}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetBaseCount(int32 InBaseCount);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	int32 GetBaseCount() const
	{
		return BaseCount;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetHeightCount(int32 InHeightCount);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	int32 GetHeightCount() const
	{
		return HeightCount;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetHeight(float InHeight);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	float GetHeight() const
	{
		return Height;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetRadius(float InRadius);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	float GetRadius() const
	{
		return Radius;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetAngleStart(float InAngleStart);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	float GetAngleStart() const
	{
		return AngleStart;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetAngleRatio(float InAngleRatio);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	float GetAngleRatio() const
	{
		return AngleRatio;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetOrientMesh(bool bInOrientMesh);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	bool GetOrientMesh() const
	{
		return bOrientMesh;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetPlane(EAvaClonerPlane InPlane);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	EAvaClonerPlane GetPlane() const
	{
		return Plane;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetRotation(const FRotator& InRotation);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
	const FRotator& GetRotation() const
	{
		return Rotation;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Cylinder")
	void SetScale(const FVector& InScale);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Cylinder")
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
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetBaseCount", Getter="GetBaseCount", Category="Layout")
	int32 BaseCount = 3 * 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetHeightCount", Getter="GetHeightCount", Category="Layout")
	int32 HeightCount = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetHeight", Getter="GetHeight", Category="Layout")
	float Height = 400.f;
	
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
	static const TAvaPropertyChangeDispatcher<UAvaClonerCylinderLayout> PropertyChangeDispatcher;
#endif
};