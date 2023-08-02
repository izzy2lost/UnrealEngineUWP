// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDStaticMeshComponent.h"

void UChaosVDStaticMeshComponent::UpdateVisibility()
{
	UpdateVisibility_Internal(CollisionData, Cast<UMeshComponent>(this));
}

void UChaosVDStaticMeshComponent::UpdateDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray)
{
	UpdateDataFromShapeArray_Internal(InShapeArray, CollisionData);
}

void UChaosVDStaticMeshComponent::SetRootImplicitObject(const Chaos::FConstImplicitObjectPtr& InImplicitObject)
{
	RootImplicitObject = InImplicitObject;
}

