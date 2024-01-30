// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaClonerEffectorShared.h"
#include "AvaClonerLayoutBase.h"
#include "AvaPropertyChangeDispatcher.h"
#include "AvaClonerGridLayout.generated.h"

UCLASS(BlueprintType)
class AVALANCHEEFFECTORS_API UAvaClonerGridLayout : public UAvaClonerLayoutBase
{
	GENERATED_BODY()

	friend class FAvaClonerActorVisualizer;
	
public:
	UAvaClonerGridLayout()
		: UAvaClonerLayoutBase(
			TEXT("Grid")
			, TEXT("/Script/Niagara.NiagaraSystem'/Avalanche/ClonerResources/Systems/NS_ClonerGrid.NS_ClonerGrid'")
		)
	{}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetCountX(int32 InCountX);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	int32 GetCountX() const
	{
		return CountX;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetCountY(int32 InCountY);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	int32 GetCountY() const
	{
		return CountY;
	}
	
	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetCountZ(int32 InCountZ);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	int32 GetCountZ() const
	{
		return CountZ;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetSpacingX(float InSpacingX);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	float GetSpacingX() const
	{
		return SpacingX;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetSpacingY(float InSpacingY);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	float GetSpacingY() const
	{
		return SpacingY;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetSpacingZ(float InSpacingZ);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	float GetSpacingZ() const
	{
		return SpacingZ;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetConstraint(EAvaClonerGridConstraint InConstraint);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	EAvaClonerGridConstraint GetConstraint() const
	{
		return Constraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetInvertConstraint(bool bInInvertConstraint);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	bool GetInvertConstraint() const
	{
		return bInvertConstraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetSphereConstraint(const FAvaClonerGridConstraintSphere& InConstraint);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	const FAvaClonerGridConstraintSphere& GetSphereConstraint() const
	{
		return SphereConstraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetCylinderConstraint(const FAvaClonerGridConstraintCylinder& InConstraint);

	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	const FAvaClonerGridConstraintCylinder& GetCylinderConstraint() const
	{
		return CylinderConstraint;
	}

	UFUNCTION(BlueprintCallable, Category="Cloner|Layout|Grid")
	void SetTextureConstraint(const FAvaClonerGridConstraintTexture& InConstraint);
	
	UFUNCTION(BlueprintPure, Category="Cloner|Layout|Grid")
	const FAvaClonerGridConstraintTexture& GetTextureConstraint() const
	{
		return TextureConstraint;
	}
	
	//~ Begin UObject
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~ End UObject

protected:
	virtual void OnLayoutParametersChanged(UAvaClonerComponent* InComponent) override;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCountX", Getter="GetCountX", Category="Layout")
	int32 CountX = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCountY", Getter="GetCountY", Category="Layout")
	int32 CountY = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCountZ", Getter="GetCountZ", Category="Layout")
	int32 CountZ = 3;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacingX", Getter="GetSpacingX", Category="Layout")
	float SpacingX = 105.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacingY", Getter="GetSpacingY", Category="Layout")
	float SpacingY = 105.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSpacingZ", Getter="GetSpacingZ", Category="Layout")
	float SpacingZ = 105.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetConstraint", Getter="GetConstraint", Category="Layout")
	EAvaClonerGridConstraint Constraint = EAvaClonerGridConstraint::None;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetInvertConstraint", Getter="GetInvertConstraint", Category="Layout", meta=(EditCondition="Constraint != EAvaClonerGridConstraint::None", EditConditionHides))
	bool bInvertConstraint = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetSphereConstraint", Getter="GetSphereConstraint", Category="Layout", meta=(EditCondition="Constraint == EAvaClonerGridConstraint::Sphere", EditConditionHides))
	FAvaClonerGridConstraintSphere SphereConstraint;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetCylinderConstraint", Getter="GetCylinderConstraint", Category="Layout", meta=(EditCondition="Constraint == EAvaClonerGridConstraint::Cylinder", EditConditionHides))
	FAvaClonerGridConstraintCylinder CylinderConstraint;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Setter="SetTextureConstraint", Getter="GetTextureConstraint", Category="Layout", meta=(EditCondition="Constraint == EAvaClonerGridConstraint::Texture", EditConditionHides))
	FAvaClonerGridConstraintTexture TextureConstraint;

private:
#if WITH_EDITOR
	/** Used for PECP */
	static const TAvaPropertyChangeDispatcher<UAvaClonerGridLayout> PropertyChangeDispatcher;
#endif
};