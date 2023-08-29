// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterInFrustumFitCameraComponent.h"
#include "Components/BillboardComponent.h"

#include "CineCameraActor.h"
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"

#include "DisplayClusterWarpLog.h"
#include "DisplayClusterWarpStrings.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterWarpBlend.h"

#include "Render/IDisplayClusterRenderManager.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Warp/IDisplayClusterWarpPolicyFactory.h"
#include "Render/Warp/IDisplayClusterWarpPolicy.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterPreviewComponent.h"

namespace UE::DisplayClusterWarp::ViewPointComponent
{
	static inline TSharedPtr<IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> ImplCreateWarpPolicy(const FString& InWarpPolicyType, const FString& InWarpPolicyName)
	{
		static IDisplayCluster& DisplayClusterSingleton = IDisplayCluster::Get();
		if (IDisplayClusterRenderManager* const DCRenderManager = DisplayClusterSingleton.GetRenderMgr())
		{
			TSharedPtr<IDisplayClusterWarpPolicyFactory> WarpPolicyFactory = DCRenderManager->GetWarpPolicyFactory(InWarpPolicyType);
			if (WarpPolicyFactory.IsValid())
			{
				return WarpPolicyFactory->Create(InWarpPolicyType, InWarpPolicyName);
			}
		}

		return nullptr;
	}
};
using namespace UE::DisplayClusterWarp::ViewPointComponent;

//--------------------------------------------------------------------------------
// UDisplayClusterInFrustumFitCameraComponent
//--------------------------------------------------------------------------------
UDisplayClusterInFrustumFitCameraComponent::UDisplayClusterInFrustumFitCameraComponent(const FObjectInitializer& ObjectInitializer)
	: UDisplayClusterCameraComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
}

void UDisplayClusterInFrustumFitCameraComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// todo: render preview

#if WITH_EDITOR
	if (WarpPolicy.IsValid())
	{
		if (bRefreshPreviewState)
		{
			// Set the preview mesh visibility state to match the bShowPreviewFrustumFit flag
			if (ADisplayClusterRootActor* ParentRootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
			{
				if (IDisplayClusterViewportManager* ViewportManager = ParentRootActor->GetViewportManager())
				{
					TArray<TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>> Viewports = ViewportManager->GetViewportsForWarpPolicy(WarpPolicy);
					for (const TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe>& Viewport : Viewports)
					{
						if (UMeshComponent* MovableMeshComponent = Viewport->GetProjectionPolicy()->GetOrCreatePreviewMovableMeshComponent(Viewport.Get()))
						{
							MovableMeshComponent->SetVisibility(bShowPreviewFrustumFit);
						}
					}
				}
			}

			bRefreshPreviewState = false;
		}
	}
#endif

	// Tick warp policy instance
	if (WarpPolicy.IsValid())
	{
		if (ADisplayClusterRootActor* ParentRootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
		{
			if (IDisplayClusterViewportManager* ViewportManager = ParentRootActor->GetViewportManager())
			{
				WarpPolicy->Tick(ViewportManager, DeltaTime);
			}
		}
	}
}

void UDisplayClusterInFrustumFitCameraComponent::GetDesiredView(FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane)
{
	if (IsEnabled())
	{
		if (ADisplayClusterRootActor* ParentRootActor = Cast<ADisplayClusterRootActor>(GetOwner()))
		{
			if (IDisplayClusterViewport::GetCameraComponentView(GetExternalCameraComponent(), ParentRootActor->GetWorldDeltaSeconds(), bUseCameraPostprocess, InOutViewInfo, OutCustomNearClippingPlane))
			{
				// 1. Use external camera for rendering
				return;
			}

			if(IDisplayClusterViewportManager* ViewportManager = ParentRootActor->GetViewportManager())
			{
				if (IDisplayClusterViewport::GetPlayerCameraView(ViewportManager->GetCurrentWorld(), bUseCameraPostprocess, InOutViewInfo))
				{
					// 2. Use active game camera
					return;
				}
			}
		}
	}

	// use default logic
	return UDisplayClusterCameraComponent::GetDesiredView(InOutViewInfo, OutCustomNearClippingPlane);
}

bool UDisplayClusterInFrustumFitCameraComponent::ShouldUseEntireClusterViewports(IDisplayClusterViewportManager* InViewportManager) const
{
	// Only when this component is enabled should viewports be created for the entire cluster that accesses this component.
	return IsEnabled();
}

IDisplayClusterWarpPolicy* UDisplayClusterInFrustumFitCameraComponent::GetWarpPolicy(IDisplayClusterViewportManager* InViewportManager)
{
	// We can ask for different types of warp policies, depending on the rules of the user settings
	const FString NewWaprPolicyType = DisplayClusterWarpStrings::warp::InFrustumFit;

	// when returns different type, this will recreate warp policy instance
	if (WarpPolicy.IsValid() && WarpPolicy->GetType() != NewWaprPolicyType)
	{
		WarpPolicy.Reset();
	}

	if (!WarpPolicy.IsValid())
	{
		WarpPolicy = ImplCreateWarpPolicy(NewWaprPolicyType, GetName());
	}

	return WarpPolicy.Get();
}

void UDisplayClusterInFrustumFitCameraComponent::OnRegister()
{
	Super::OnRegister();


#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		if (SpriteComponent)
		{
			SpriteComponent->SpriteInfo.Category = TEXT("NDisplayCameraViewOrigin");
			SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("DisplayClusterInFrustumFitCameraComponent", "DisplayClusterInFrustumFitCameraComponentSpriteInfo", "nDisplay InFrustumFit View Origin");
		}
	}

	RefreshVisualRepresentation();
#endif

}

bool UDisplayClusterInFrustumFitCameraComponent::IsEnabled() const
{
	return bEnableCameraProjection;
}

UCameraComponent* UDisplayClusterInFrustumFitCameraComponent::GetExternalCameraComponent() const
{
	if (ACineCameraActor* CineCamera = ExternalCameraActor.Get())
	{
		return CineCamera->GetCameraComponent();
	}

	return nullptr;
}

#if WITH_EDITOR
void UDisplayClusterInFrustumFitCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UDisplayClusterInFrustumFitCameraComponent, bShowPreviewFrustumFit))
	{
		InvalidatePreviewState();
	}
}

bool UDisplayClusterInFrustumFitCameraComponent::GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut)
{
	if (UCameraComponent* CameraComponent = GetExternalCameraComponent())
	{
		return CameraComponent->GetEditorPreviewInfo(DeltaTime, ViewOut);
	}

	return false;
}

TSharedPtr<SWidget> UDisplayClusterInFrustumFitCameraComponent::GetCustomEditorPreviewWidget()
{
	if (UCameraComponent* CameraComponent = GetExternalCameraComponent())
	{
		return CameraComponent->GetCustomEditorPreviewWidget();
	}

	return nullptr;
}

#endif
