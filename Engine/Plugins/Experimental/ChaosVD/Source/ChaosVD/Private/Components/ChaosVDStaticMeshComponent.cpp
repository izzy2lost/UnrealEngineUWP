// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/ChaosVDStaticMeshComponent.h"

void UChaosVDStaticMeshComponent::UpdateVisibility()
{
	UpdateVisibility_Internal(CollisionData, Cast<UMeshComponent>(this));
}

void UChaosVDStaticMeshComponent::UpdateDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray)
{
	UpdateDataFromShapeArray_Internal(InShapeArray, CollisionData, this);
}

void UChaosVDStaticMeshComponent::SetImplicitObject(const Chaos::FImplicitObject* InImplicitObject)
{
	SetImplicitObject_Internal(InImplicitObject);
}

void UChaosVDStaticMeshComponent::UpdateColors()
{
	UpdateColors_Internal(this);
}

void UChaosVDStaticMeshComponent::SetRootImplicitObject(const Chaos::FConstImplicitObjectPtr& InImplicitObject)
{
	RootImplicitObject = InImplicitObject;
}

