// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDInstancedStaticMeshComponent.h"

void UChaosVDInstancedStaticMeshComponent::UpdateVisibility()
{
	UpdateVisibility_Internal(CollisionData, Cast<UMeshComponent>(this));
}

void UChaosVDInstancedStaticMeshComponent::UpdateDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray)
{
	UpdateDataFromShapeArray_Internal(InShapeArray, CollisionData);
}

void UChaosVDInstancedStaticMeshComponent::SetRootImplicitObject(const Chaos::FConstImplicitObjectPtr& InImplicitObject)
{
	RootImplicitObject = InImplicitObject;
}

