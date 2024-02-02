// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoverBasePawn.h"
#include "MoverComponent.h"


static const FName Name_CharacterMotionComponent(TEXT("MoverComponent"));


AMoverBasePawn::AMoverBasePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	CharacterMotionComponent = CreateDefaultSubobject<UMoverComponent>(Name_CharacterMotionComponent);
	ensure(CharacterMotionComponent);

 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SetReplicatingMovement(false);	// disable Actor-level movement replication, since our Mover component will handle it
}



UPrimitiveComponent* AMoverBasePawn::GetMovementBase() const
{
	return CharacterMotionComponent ? CharacterMotionComponent->GetMovementBase() : nullptr;
}


