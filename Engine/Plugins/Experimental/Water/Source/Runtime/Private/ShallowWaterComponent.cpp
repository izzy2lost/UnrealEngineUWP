// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShallowWaterComponent.h"

#include "ShallowWaterSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "WaterBodyActor.h"
#include "WaterSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"

UShallowWaterComponent::UShallowWaterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickEnabled(true);
	SetUsingAbsoluteRotation(true);
}

void UShallowWaterComponent::BeginPlay()
{
	Super::BeginPlay();

	GetOwner()->OnActorBeginOverlap.AddDynamic(this, &UShallowWaterComponent::OnOwnerBeginOverlap);
	TArray<USkeletalMeshComponent*> SKMs;
	GetOwner()->GetComponents<USkeletalMeshComponent>(SKMs);
	for(USkeletalMeshComponent* SKM : SKMs)
	{
		SKM->ComponentTags.Add(FName("RigidMesh_ShallowWaterCollider"));
	}

	ShallowWaterNiagaraSimulation = UNiagaraFunctionLibrary::SpawnSystemAttached(GetDefault<UShallowWaterSettings>()->DefaultShallowWaterNiagaraSimulation.LoadSynchronous(), this, FName(""),
		FVector::Zero(), FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
	ShallowWaterNiagaraSimulation->SetGpuComputeDebug(GetDefault<UShallowWaterSettings>()->bGPUComputeDebug);
}

void UShallowWaterComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if(CurrentWaterBody == nullptr)
	{
		return;
	}

	// #todo this query gets the closest water surface in 3D space, not a XY-plane projection as we instinctively assumed
	// Could be better and could be worse. We are already ignoring the accurate character-capsule-in-water-plane collision volume
	const FWaterBodyQueryResult WaterInfo = CurrentWaterBody->GetWaterBodyComponent()->QueryWaterInfoClosestToWorldLocation(
		GetOwner()->GetActorLocation(), EWaterBodyQueryFlags::ComputeLocation);
	const FVector WaterLocation = WaterInfo.GetWaterSurfaceLocation();
	SetWorldLocation(FVector(GetOwner()->GetActorLocation().X, GetOwner()->GetActorLocation().Y, WaterLocation.Z));
}

void UShallowWaterComponent::OnRegister()
{
	Super::OnRegister();
}

void UShallowWaterComponent::OnOwnerBeginOverlap(AActor* Owner, AActor* OtherActor)
{
	AWaterBody* OtherWaterBody = Cast<AWaterBody>(OtherActor);
	if(OtherWaterBody != nullptr && OtherWaterBody != CurrentWaterBody)
	{
		CurrentWaterBody = OtherWaterBody;
		UpdateNiagaraVariables();
	}
}

void UShallowWaterComponent::UpdateNiagaraVariables()
{
	TObjectPtr<UTextureRenderTarget2D> WaterInfoTexture = CurrentWaterBody->GetWaterBodyComponent()->GetWaterZone()->
	                                                                        WaterInfoTexture;
	ShallowWaterNiagaraSimulation->SetVariableTexture(FName("WaterInfoTexture"), Cast<UTexture>(WaterInfoTexture));
}

