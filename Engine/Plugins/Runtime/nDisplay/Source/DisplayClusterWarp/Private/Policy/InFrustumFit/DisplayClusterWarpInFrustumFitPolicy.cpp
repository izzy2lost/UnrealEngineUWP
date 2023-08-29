// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/InFrustumFit/DisplayClusterWarpInFrustumFitPolicy.h"

#include "DisplayClusterRootActor.h"
#include "Containers/DisplayClusterWarpEye.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "IDisplayClusterWarpBlend.h"

#include "DisplayClusterWarpStrings.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterInFrustumFitCameraComponent.h"
#include "Components/DisplayClusterPreviewComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Experimental: Enable static view direction for mpcdi 2d
int32 GDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D = 1;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D(
	TEXT("nDisplay.warp.InFrustumFit.UseStaticViewDirectionForMPCDIProfile2D"),
	GDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D,
	TEXT("Experimental: Enable static view direction for mpcdi 2d (0 - disable)\n"),
	ECVF_Default
);

// Experimental: Enable single view target for group of viewports
int32 GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget = 1;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget(
	TEXT("nDisplay.warp.InFrustumFit.UseGroupViewTarget"),
	GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget,
	TEXT("Experimental: Enable single view target for group of viewports (0 - disable)\n"),
	ECVF_Default
);

int32 GDisplayClusterWarpInFrustumFitPolicyDrawFrustum = 0;
static FAutoConsoleVariableRef CVarDisplayClusterWarpInFrustumFitPolicyDrawFrustum(
	TEXT("nDisplay.warp.InFrustumFit.DrawFrustum"),
	GDisplayClusterWarpInFrustumFitPolicyDrawFrustum,
	TEXT("Toggles drawing the stage geometry frustum and bounding box\n"),
	ECVF_Default
);

//-------------------------------------------------------------------
// FDisplayClusterWarpInFrustumFitPolicy
//-------------------------------------------------------------------
FDisplayClusterWarpInFrustumFitPolicy::FDisplayClusterWarpInFrustumFitPolicy(const FString& InWarpPolicyName)
	: FDisplayClusterWarpPolicyBase(GetType(), InWarpPolicyName)
{ }

const FString& FDisplayClusterWarpInFrustumFitPolicy::GetType() const
{
	static const FString Type(DisplayClusterWarpStrings::warp::InFrustumFit);

	return Type;
}

void FDisplayClusterWarpInFrustumFitPolicy::HandleNewFrame(const TArray<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>>& InViewports)
{
	// Todo: get real scale
	const float WorldToMeters = 100.0f;

	const float WorldScale = WorldToMeters / 100.f;
	
	bGeometryContextsUpdated = false;
	if (GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget)
	{
		// Calculate AABB for group of viewports:
		GroupAABBox = FDisplayClusterWarpAABB();
		for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : InViewports)
		{
			if (Viewport.IsValid() && Viewport->GetProjectionPolicy().IsValid())
			{
				TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
				if (Viewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
				{
					if (WarpBlend->UpdateGeometryContext(WorldScale))
					{
						GroupAABBox.UpdateAABB(WarpBlend->GetGeometryContext().AABBox);
					}
				}
			}
		}

		bGeometryContextsUpdated = true;
	}

	const uint32 ContextNum = 0; // calculate for a single context

	bValidGroupFrustum = false;

	// Recalculate warp projection angles
	GroupGeometryWarpProjection.ResetProjectionAngles();
	FittedViewports.Empty();
	SymmetricForwardCorrection.Reset();

	bool bCanCalcFrustumContext = true;
	bool bHasFixedViewDirection = false;

	// Use center of GroupAABB as target viewpoint, and setup to use same viewprojection plane for all viewports:
	for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : InViewports)
	{
		if (bCanCalcFrustumContext && Viewport.IsValid() && Viewport->GetProjectionPolicy().IsValid())
		{
			// Note: This code is partially copied from FDisplayClusterProjectionMPCDIPolicy::CalculateView().

			// Override viewpoint
			// MPCDI always expects the location of the viewpoint component (eye location from the real world)
			FVector ViewOffset = FVector::ZeroVector;
			FVector InOutViewLocation;
			FRotator InOutViewRotation;
			if (!Viewport->GetViewPointCameraEye(ContextNum, InOutViewLocation, InOutViewRotation, ViewOffset))
			{
				continue;
			}

			if (UDisplayClusterInFrustumFitCameraComponent* WarpViewPointComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(Viewport->GetViewPointCameraComponent()))
			{
				bHasFixedViewDirection = WarpViewPointComponent->CameraViewTarget == EDisplayClusterWarpCameraViewTarget::MatchViewOrigin;
			}

			TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
			if (Viewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
			{
				const USceneComponent* const OriginComp = Viewport->GetProjectionPolicy()->GetOriginComponent();

				TSharedPtr<FDisplayClusterWarpEye, ESPMode::ThreadSafe> WarpEye = MakeShared<FDisplayClusterWarpEye, ESPMode::ThreadSafe>(Viewport, 0);

				WarpEye->World2LocalTransform = (OriginComp ? OriginComp->GetComponentTransform() : FTransform::Identity);

				// Get our base camera location and view offset in local space (MPCDI space)
				WarpEye->ViewPoint.Location = WarpEye->World2LocalTransform.InverseTransformPosition(InOutViewLocation - ViewOffset);
				WarpEye->ViewPoint.EyeOffset = WarpEye->World2LocalTransform.InverseTransformPosition(InOutViewLocation) - WarpEye->ViewPoint.Location;
				WarpEye->ViewPoint.Rotation = WarpEye->World2LocalTransform.InverseTransformRotation(InOutViewRotation.Quaternion()).Rotator();

				WarpEye->WorldScale = WorldScale;

				WarpEye->WarpPolicy = SharedThis(this);

				if (!WarpBlend->CalcFrustumContext(WarpEye))
				{
					bCanCalcFrustumContext = false;
				}
				else
				{
					const FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(ContextNum);
					GroupGeometryWarpProjection.ExpandProjectionAngles(WarpData.GeometryWarpProjection);
				}
			}
		}
	}

	bValidGroupFrustum = bCanCalcFrustumContext;

	// Recalculate the group frustum so that it is symmetric
	MakeGroupFrustumSymmetrical(bHasFixedViewDirection);
}

void FDisplayClusterWarpInFrustumFitPolicy::Tick(IDisplayClusterViewportManager* InViewportManager, float DeltaSeconds)
{
#if WITH_EDITOR
	bool bHasDrawnDebugFrustum = false;

	TArray<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>> Viewports = InViewportManager->GetViewportsForWarpPolicy(SharedThis(this));
	for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : Viewports)
	{
		// If the viewport doesn't have a valid warp projection yet, skip it
		if (!FittedViewports.Contains(Viewport->GetId()))
		{
			continue;
		}

		TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
		if (Viewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
		{
			FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(0);

			const FTransform CameraTransform(WarpData.WarpProjection.CameraRotation.Quaternion(), WarpData.WarpProjection.CameraLocation);

			const float HScale = (WarpData.WarpProjection.Left - WarpData.WarpProjection.Right) / (WarpData.GeometryWarpProjection.Left - WarpData.GeometryWarpProjection.Right);
			const float VScale = (WarpData.WarpProjection.Top - WarpData.WarpProjection.Bottom) / (WarpData.GeometryWarpProjection.Top - WarpData.GeometryWarpProjection.Bottom);

			checkf(FMath::IsNearlyEqual(HScale, VScale), TEXT("Streching the stage geometry to fit a different aspect ratio is not supported!"));

			const FVector Scale = FVector(1, HScale, HScale);

			// Compute the relative transform from the view origin to the geometry
			FTransform RelativeTransform(WarpData.WarpContext.MeshToStageMatrix * WarpData.Local2World.Inverse());
			RelativeTransform.ScaleTranslation(Scale);

			// Final transform is computed from the relative transform of the geometry to the view point, the frustum fit transform
			// which will scale and position the geometry based on the fitted frustum, and the camera transform
			const FTransform FinalTransform = RelativeTransform * CameraTransform;

			UMeshComponent* MovableMeshComponent = Viewport->GetProjectionPolicy()->GetOrCreatePreviewMovableMeshComponent(Viewport.Get());
			MovableMeshComponent->SetRelativeTransform(FinalTransform);

			// Since the mesh needs to be skewed to scale appropriately, and since Unreal Engine does not support a skew transform
			// through FTransform, the mesh needs to be skewed through the vertex shader using WorldPositionOffset,
			// so pass in the "global" scale to the preview mesh's material instance
			if (UMaterialInstanceDynamic* MaterialInstance = Cast<UMaterialInstanceDynamic>(MovableMeshComponent->GetMaterial(0)))
			{
				FMatrix CameraBasis = FRotationMatrix::Make(WarpData.WarpProjection.CameraRotation).Inverse();

				MaterialInstance->SetVectorParameterValue(TEXT("GlobalScale"), Scale);
				MaterialInstance->SetVectorParameterValue(TEXT("GlobalForward"), CameraBasis.GetUnitAxis(EAxis::X));
				MaterialInstance->SetVectorParameterValue(TEXT("GlobalRight"), CameraBasis.GetUnitAxis(EAxis::Y));
				MaterialInstance->SetVectorParameterValue(TEXT("GlobalUp"), CameraBasis.GetUnitAxis(EAxis::Z));
			}

			if (GDisplayClusterWarpInFrustumFitPolicyDrawFrustum && !bHasDrawnDebugFrustum)
			{
				DrawDebugGroupFrustum(InViewportManager->GetRootActor(), Cast<UDisplayClusterInFrustumFitCameraComponent>(Viewport->GetViewPointCameraComponent()), FColor::Blue);
				bHasDrawnDebugFrustum = true;
			}
		}
	}

	if (GDisplayClusterWarpInFrustumFitPolicyDrawFrustum)
	{
		DrawDebugGroupBoundingBox(InViewportManager->GetRootActor(), FColor::Red);
	}
#endif
}

void FDisplayClusterWarpInFrustumFitPolicy::BeginCalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum)
{
	if (InViewport && InViewport->GetProjectionPolicy().IsValid())
	{
		TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
		if (InViewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
		{
			if (UDisplayClusterInFrustumFitCameraComponent* WarpViewPointComponent = Cast<UDisplayClusterInFrustumFitCameraComponent>(InViewport->GetViewPointCameraComponent()))
			{
				FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(ContextNum);
				if (WarpData.WarpEye.IsValid())
				{
					// geometry context already updated.
					WarpData.WarpEye->bUpdateGeometryContext = !bGeometryContextsUpdated;

					if (WarpViewPointComponent->CameraViewTarget == EDisplayClusterWarpCameraViewTarget::MatchViewOrigin)
					{
						WarpData.WarpEye->OverrideViewDirection = WarpData.WarpEye->ViewPoint.Rotation.RotateVector(FVector::XAxisVector);
					}
					else
					{
						if (GDisplayClusterWarpInFrustumFitPolicyUseGroupViewTarget)
						{
							// If we have a correction to the view forward vector to make the frustum symmetric, apply it
							if (SymmetricForwardCorrection.IsSet())
							{
								const FVector AABBForward = (GroupAABBox.GetCenter() - WarpData.WarpEye->ViewPoint.GetEyeLocation()).GetSafeNormal();
								WarpData.WarpEye->OverrideViewDirection = SymmetricForwardCorrection->RotateVector(AABBForward);
							}
							else
							{
								// Use the same view target for all viewports
								WarpData.WarpEye->OverrideViewTarget = GroupAABBox.GetCenter();
							}
						}
					}

					if (GDisplayClusterWarpInFrustumFitPolicyUseStaticViewDirectionForMPCDIProfile2D)
					{
						// [experimental] use static view direction for MPCDI profile 2D
						if (WarpBlend->GetWarpProfileType() == EDisplayClusterWarpProfileType::warp_2D)
						{
							WarpData.WarpEye->OverrideViewDirection = FVector(1, 0, 0);
						}
					}

					// Todo: This feature now not supported here. need to be fixed.
					WarpData.bEnabledRotateFrustumToFitContextSize = false;
				}
			}
		}
	}
}

void FDisplayClusterWarpInFrustumFitPolicy::EndCalcFrustum(IDisplayClusterViewport* InViewport, const uint32 ContextNum)
{
	if (bValidGroupFrustum && InViewport && InViewport->GetProjectionPolicy().IsValid())
	{
		TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> WarpBlend;
		if (InViewport->GetProjectionPolicy()->GetWarpBlendInterface(WarpBlend))
		{
			if (UDisplayClusterCameraComponent* ViewPointComponent = InViewport->GetViewPointCameraComponent())
			{
				if (ViewPointComponent->IsA<UDisplayClusterInFrustumFitCameraComponent>())
				{
					if (UDisplayClusterInFrustumFitCameraComponent* WarpViewPointComponent = CastChecked<UDisplayClusterInFrustumFitCameraComponent>(ViewPointComponent))
					{
						// Change warp settings:
						FDisplayClusterWarpData& WarpData = WarpBlend->GetWarpData(ContextNum);

						// Apply camera frustum fitting:
						FDisplayClusterWarpProjection NewWarpProjection = ApplyInFrustumFit(WarpViewPointComponent, WarpData.WarpEye->World2LocalTransform, WarpData.WarpProjection);

						WarpData.WarpProjection = NewWarpProjection;

						FittedViewports.Add(InViewport->GetId());
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void FDisplayClusterWarpInFrustumFitPolicy::DrawDebugGroupBoundingBox(ADisplayClusterRootActor* RootActor, const FColor& Color)
{
	if (RootActor)
	{
		if (UWorld* World = RootActor->GetWorld())
		{
			FBox WorldBox = GroupAABBox.TransformBy(RootActor->GetActorTransform());
			DrawDebugBox(World, WorldBox.GetCenter(), WorldBox.GetExtent(), Color);
			DrawDebugPoint(World, WorldBox.GetCenter(), 5, Color);
		}
	}
}

void FDisplayClusterWarpInFrustumFitPolicy::DrawDebugGroupFrustum(ADisplayClusterRootActor* RootActor, UDisplayClusterInFrustumFitCameraComponent* CameraComponent, const FColor& Color)
{
	if (RootActor && CameraComponent)
	{
		if (UWorld* World = RootActor->GetWorld())
		{
			const float NearPlane = 10;
			const float FarPlane = 1000;

			const FVector CameraLoc = CameraComponent->GetComponentLocation();
			FVector ViewDirection;

			if (CameraComponent->CameraViewTarget == EDisplayClusterWarpCameraViewTarget::MatchViewOrigin)
			{
				ViewDirection = CameraComponent->GetComponentRotation().RotateVector(FVector::XAxisVector);
			}
			else
			{
				const FBox WorldBox = GroupAABBox.TransformBy(RootActor->GetActorTransform());
				ViewDirection = (WorldBox.GetCenter() - CameraComponent->GetComponentLocation()).GetSafeNormal();

				if (SymmetricForwardCorrection.IsSet())
				{
					ViewDirection = SymmetricForwardCorrection->RotateVector(ViewDirection);
				}
			}

			DrawDebugLine(World, CameraLoc, CameraLoc + ViewDirection * 50, Color);

			const FRotator ViewRotator = ViewDirection.ToOrientationRotator();
			const FVector FrustumTopLeft = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Left, GroupGeometryWarpProjection.Top) / GroupGeometryWarpProjection.ZNear);
			const FVector FrustumTopRight = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Right, GroupGeometryWarpProjection.Top) / GroupGeometryWarpProjection.ZNear);
			const FVector FrustumBottomLeft = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Left, GroupGeometryWarpProjection.Bottom) / GroupGeometryWarpProjection.ZNear);
			const FVector FrustumBottomRight = ViewRotator.RotateVector(FVector(GroupGeometryWarpProjection.ZNear, GroupGeometryWarpProjection.Right, GroupGeometryWarpProjection.Bottom) / GroupGeometryWarpProjection.ZNear);

			const FVector FrustumVertices[8] =
			{
				CameraLoc + FrustumTopLeft * NearPlane,
				CameraLoc + FrustumTopRight * NearPlane,
				CameraLoc + FrustumBottomRight * NearPlane,
				CameraLoc + FrustumBottomLeft * NearPlane,

				CameraLoc + FrustumTopLeft * FarPlane,
				CameraLoc + FrustumTopRight * FarPlane,
				CameraLoc + FrustumBottomRight * FarPlane,
				CameraLoc + FrustumBottomLeft * FarPlane,
			};

			// Near plane rectangle
			DrawDebugLine(World, FrustumVertices[0], FrustumVertices[1], Color);
			DrawDebugLine(World, FrustumVertices[1], FrustumVertices[2], Color);
			DrawDebugLine(World, FrustumVertices[2], FrustumVertices[3], Color);
			DrawDebugLine(World, FrustumVertices[3], FrustumVertices[0], Color);

			// Frustum
			DrawDebugLine(World, FrustumVertices[0], FrustumVertices[4], Color);
			DrawDebugLine(World, FrustumVertices[1], FrustumVertices[5], Color);
			DrawDebugLine(World, FrustumVertices[2], FrustumVertices[6], Color);
			DrawDebugLine(World, FrustumVertices[3], FrustumVertices[7], Color);

			// Far plane rectangle
			DrawDebugLine(World, FrustumVertices[4], FrustumVertices[5], Color);
			DrawDebugLine(World, FrustumVertices[5], FrustumVertices[6], Color);
			DrawDebugLine(World, FrustumVertices[6], FrustumVertices[7], Color);
			DrawDebugLine(World, FrustumVertices[7], FrustumVertices[4], Color);
		}
	}
}
#endif
