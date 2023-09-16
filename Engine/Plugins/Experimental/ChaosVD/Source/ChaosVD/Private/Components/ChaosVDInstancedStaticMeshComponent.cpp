// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDInstancedStaticMeshComponent.h"

void UChaosVDInstancedStaticMeshComponent::UpdateVisibility()
{
	UpdateVisibility_Internal(CollisionData, Cast<UMeshComponent>(this));
}

void UChaosVDInstancedStaticMeshComponent::UpdateDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray)
{
	UpdateDataFromShapeArray_Internal(InShapeArray, CollisionData, this);
}

void UChaosVDInstancedStaticMeshComponent::SetImplicitObject(const Chaos::FImplicitObject* InImplicitObject)
{
	SetImplicitObject_Internal(InImplicitObject);
}

void UChaosVDInstancedStaticMeshComponent::UpdateColors()
{
	UpdateColors_Internal(this);
}

void UChaosVDInstancedStaticMeshComponent::SetRootImplicitObject(const Chaos::FConstImplicitObjectPtr& InImplicitObject)
{
	RootImplicitObject = InImplicitObject;
}

