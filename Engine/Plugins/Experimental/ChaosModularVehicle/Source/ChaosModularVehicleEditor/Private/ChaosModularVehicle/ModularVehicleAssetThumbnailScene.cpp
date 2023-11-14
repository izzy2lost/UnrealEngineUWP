// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosModularVehicle/ModularVehicleAssetThumbnailScene.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ChaosModularVehicle/ModularVehicleAsset.h"
#include "ChaosModularVehicle/ModularVehicleComponent.h"
#include "ChaosModularVehicle/ModularVehiclePawn.h"

FModularVehicleAssetThumbnailScene::FModularVehicleAssetThumbnailScene()
{
	bForceAllUsedMipsResident = false;

	// Create preview actor
	// checked
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;

	PreviewActor = GetWorld()->SpawnActor<AModularVehiclePawn>(SpawnInfo);
	PreviewActor->GetModularVehicleComponent()->SetMobility(EComponentMobility::Movable);
	PreviewActor->SetActorEnableCollision(false);
}

void FModularVehicleAssetThumbnailScene::SetModularVehicleAsset(UModularVehicleAsset* ModularVehicleAsset)
{
	//PreviewActor->GetModularVehicleComponent()->SetVehicleAsset(ModularVehicleAsset);

	if (ModularVehicleAsset)
	{
		FTransform MeshTransform = FTransform::Identity;

		PreviewActor->SetActorLocation(FVector(0, 0, 0), false);
		PreviewActor->GetModularVehicleComponent()->UpdateBounds();

		// Center the mesh at the world origin then offset to put it on top of the plane
		const float BoundsZOffset = GetBoundsZOffset(PreviewActor->GetModularVehicleComponent()->Bounds);
		PreviewActor->SetActorLocation(-PreviewActor->GetModularVehicleComponent()->Bounds.Origin + FVector(0, 0, BoundsZOffset), false);
		PreviewActor->GetModularVehicleComponent()->RecreateRenderState_Concurrent();
	}
}

void FModularVehicleAssetThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(PreviewActor);
	check(PreviewActor->GetModularVehicleComponent());
	//check(PreviewActor->GetModularVehicleComponent()->GetVehicleAsset());

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	// Add extra size to view slightly outside of the sphere to compensate for perspective
	const float HalfMeshSize = PreviewActor->GetModularVehicleComponent()->Bounds.SphereRadius * 1.15;
	const float BoundsZOffset = GetBoundsZOffset(PreviewActor->GetModularVehicleComponent()->Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo;// = Cast<USceneThumbnailInfo>(PreviewActor->GetModularVehicleComponent()->GetVehicleAsset()->ThumbnailInfo);
	//if (ThumbnailInfo)
	//{
	//	if (TargetDistance + ThumbnailInfo->OrbitZoom < 0)
	//	{
	//		ThumbnailInfo->OrbitZoom = -TargetDistance;
	//	}
	//}
	//else
	{
		ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	}

	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}

