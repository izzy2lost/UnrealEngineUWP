// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverComponent.h"
#include "CharacterMoverComponent.generated.h"

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class MOVER_API UCharacterMoverComponent : public UMoverComponent
{
	GENERATED_BODY()
	
public:
	UCharacterMoverComponent();
	
	virtual void BeginPlay() override;
	
	/** Returns true if currently crouching */ 
	UFUNCTION(BlueprintCallable, Category = Mover)
	virtual bool IsCrouching() const;

	/** Returns true if currently flying (moving through a non-fluid volume without resting on the ground) */
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsFlying() const;
	
	// Is this actor in a falling state? Note that this includes upwards motion induced by jumping.
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsFalling() const;

	// Is this actor in a airborne state? (e.g. Flying, Falling)
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsAirborne() const;

	// Is this actor in a grounded state? (e.g. Walking)
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsOnGround() const;

	// Is this actor in a Swimming state? (e.g. Swimming)
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsSwimming() const;
	
	// Is this actor sliding on an unwalkable slope
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool IsSlopeSliding() const;

	// Can this Actor jump?
	UFUNCTION(BlueprintPure, Category = Mover)
	virtual bool CanActorJump() const;

	// Perform jump on actor
	UFUNCTION(BlueprintCallable, Category = Mover)
	virtual bool Jump();

	// Whether this component should directly handle jumping or not 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mover)
    bool bHandleJump;
	
protected:
	UFUNCTION()
	virtual void OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd);
};
