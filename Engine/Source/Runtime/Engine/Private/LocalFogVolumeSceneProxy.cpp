// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FVolumetricCloudSceneProxy implementation.
=============================================================================*/

#include "LocalFogVolumeSceneProxy.h"
#include "Components/LocalFogVolumeComponent.h"



FLocalFogVolumeSceneProxy::FLocalFogVolumeSceneProxy(const ULocalFogVolumeComponent* InComponent)
	: FogDensity(InComponent->FogDensity)
	, FogHeightFalloff(InComponent->FogHeightFalloff)
	, FogHeightOffset(InComponent->FogHeightOffset)
	, FogUniformScale(1.0f)
	, FogMode((uint8)InComponent->FogMode)
	, FogSortPriority(uint8(127 - int8(InComponent->FogSortPriority))) // FogSortPriority on the component is in [-127,127] and needs to be negated to match expected priority behavior.
	, FogPhaseG(InComponent->FogPhaseG)
	, FogAlbedo(InComponent->FogAlbedo)
	, FogEmissive(InComponent->FogEmissive)
{
	UpdateComponentTransform(InComponent->GetComponentTransform());
}

FLocalFogVolumeSceneProxy::~FLocalFogVolumeSceneProxy()
{
}

void FLocalFogVolumeSceneProxy::UpdateComponentTransform(const FTransform& Transform)
{
	FogTransform = Transform;
	const float MaximumAxisScale = FogTransform.GetMaximumAxisScale();
	FogTransform.SetScale3D(FVector(MaximumAxisScale, MaximumAxisScale, MaximumAxisScale));

	FogUniformScale = MaximumAxisScale;
}

