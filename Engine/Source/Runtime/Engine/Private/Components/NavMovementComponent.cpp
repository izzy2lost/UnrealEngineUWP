// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/NavMovementComponent.h"
#include "AI/NavigationSystemBase.h"
#include "Components/CapsuleComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavMovementComponent)


//----------------------------------------------------------------------//
// UMovementComponent
//----------------------------------------------------------------------//
UNavMovementComponent::UNavMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bComponentShouldUpdatePhysicsVolume = true;
}

FBasedPosition UNavMovementComponent::GetActorFeetLocationBased() const
{
	return FBasedPosition(NULL, GetActorFeetLocation());
}

void UNavMovementComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	// copying over any default values to the new struct that houses the properties
	if (FixedPathBrakingDistance != 0) 
	{
		NavMovementProperties.FixedPathBrakingDistance = FixedPathBrakingDistance;
		FixedPathBrakingDistance = 0;
	}
	
	if (!bUpdateNavAgentWithOwnersCollision)
	{
		NavMovementProperties.bUpdateNavAgentWithOwnersCollision = bUpdateNavAgentWithOwnersCollision;
		bUpdateNavAgentWithOwnersCollision = true;
	}
	
	if (bUseAccelerationForPaths)
    {
    	NavMovementProperties.bUseAccelerationForPaths = bUseAccelerationForPaths;
    	bUseAccelerationForPaths = false;
    }
	
	if (bUseFixedBrakingDistanceForPaths)
    {
    	NavMovementProperties.bUseFixedBrakingDistanceForPaths = bUseFixedBrakingDistanceForPaths;
    	bUseFixedBrakingDistanceForPaths = false;
    }
    
    if (!bStopMovementAbortPaths)
    {
    	NavMovementProperties.bStopMovementAbortPaths = bStopMovementAbortPaths;
    	bStopMovementAbortPaths = true;
    }
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA	
}

void UNavMovementComponent::UpdateNavAgent(const UObject& ObjectToUpdateFrom)
{
	if (!NavMovementProperties.bUpdateNavAgentWithOwnersCollision)
	{
		return;
	}

	// initialize properties from navigation system
	NavAgentProps.NavWalkingSearchHeightScale = FNavigationSystem::GetDefaultSupportedAgent().NavWalkingSearchHeightScale;
	
	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(&ObjectToUpdateFrom))
	{
		NavAgentProps.AgentRadius = CapsuleComponent->GetScaledCapsuleRadius();
		NavAgentProps.AgentHeight = CapsuleComponent->GetScaledCapsuleHalfHeight() * 2.f;;
	}
	else if (const AActor* ObjectAsActor = Cast<AActor>(&ObjectToUpdateFrom))
	{
		ensureMsgf(&ObjectToUpdateFrom == GetOwner(), TEXT("Object passed to UpdateNavAgent should be the owner actor of the Nav Movement Component"));
		// Can't call GetSimpleCollisionCylinder(), because no components will be registered.
		float BoundRadius, BoundHalfHeight;	
		ObjectAsActor->GetSimpleCollisionCylinder(BoundRadius, BoundHalfHeight);
		NavAgentProps.AgentRadius = BoundRadius;
		NavAgentProps.AgentHeight = BoundHalfHeight * 2.f;
	}
}

void UNavMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	Velocity = MoveVelocity;
}

void UNavMovementComponent::RequestPathMove(const FVector& MoveInput)
{
	// empty in base class, requires at least PawnMovementComponent for input related operations
}

bool UNavMovementComponent::CanStopPathFollowing() const
{
	return true;
}

void UNavMovementComponent::ClearFixedBrakingDistance()
{
	NavMovementProperties.bUseFixedBrakingDistanceForPaths = false;
}

void UNavMovementComponent::GetSimpleCollisionCylinder(float& CollisionRadius, float& CollisionHalfHeight) const
{
	GetOwner()->GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
}

FVector UNavMovementComponent::GetSimpleCollisionCylinderExtent() const
{
	return GetOwner()->GetSimpleCollisionCylinderExtent();
}

FVector UNavMovementComponent::GetForwardVector() const
{
	return GetOwner()->GetActorForwardVector();
}

void UNavMovementComponent::SetUpdateNavAgentWithOwnersCollisions(bool bUpdateWithOwner)
{
	NavMovementProperties.bUpdateNavAgentWithOwnersCollision = bUpdateWithOwner;
}
