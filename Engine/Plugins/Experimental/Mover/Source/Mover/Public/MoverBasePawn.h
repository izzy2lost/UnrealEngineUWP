// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MoverBasePawn.generated.h"

class UMoverComponent;

UCLASS()
class MOVER_API AMoverBasePawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AMoverBasePawn(const FObjectInitializer& ObjectInitializer);

	/** Accessor for the actor's movement component */
	UFUNCTION(BlueprintPure, Category = Mover)
	UMoverComponent* GetMoverComponent() const { return CharacterMotionComponent; }

protected:

	virtual UPrimitiveComponent* GetMovementBase() const override;

protected:
	UPROPERTY(Category = Movement, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMoverComponent> CharacterMotionComponent;

};
