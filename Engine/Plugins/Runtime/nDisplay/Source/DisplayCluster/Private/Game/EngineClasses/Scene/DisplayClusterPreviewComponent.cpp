// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterPreviewComponent.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayDevice/DisplayClusterDisplayDeviceBaseComponent.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "DisplayDevice/DisplayClusterDisplayDeviceUtils.h"
#include "DisplayClusterRootActor.h"
#include "DisplayClusterConfigurationTypes.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/Containers/DisplayClusterViewportReadPixels.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/Resource/DisplayClusterViewportResourceSettings.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "RHI.h"
#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "CanvasTypes.h"

#include "IDisplayClusterProjection.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"


UDisplayClusterPreviewComponent::UDisplayClusterPreviewComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	bWantsInitializeComponent = true;
#endif
}

#if WITH_EDITOR
void UDisplayClusterPreviewComponent::OnComponentCreated()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::OnComponentCreated"), STAT_OnComponentCreated, STATGROUP_NDisplay);
	
	Super::OnComponentCreated();
}

void UDisplayClusterPreviewComponent::DestroyComponent(bool bPromoteChildren)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::DestroyComponent"), STAT_DestroyComponent, STATGROUP_NDisplay);
	
	ReleasePreviewMesh();
	ReleasePreviewMaterial();

	ReleasePreviewRenderTarget();

	Super::DestroyComponent(bPromoteChildren);
}

void UDisplayClusterPreviewComponent::ResetPreviewComponent(bool bInRestoreSceneMaterial)
{
	if (bInRestoreSceneMaterial)
	{
		RestorePreviewMeshMaterial();
	}
	else
	{
		UpdatePreviewMesh();
	}
}

IDisplayClusterViewport* UDisplayClusterPreviewComponent::GetCurrentViewport() const
{
	if (RootActor != nullptr)
	{
		return RootActor->FindPreviewViewport(ViewportId);
	}

	return nullptr;
}

bool UDisplayClusterPreviewComponent::InitializePreviewComponent(ADisplayClusterRootActor* InRootActor, const FString& InClusterNodeId,
	const FString& InViewportId, UDisplayClusterConfigurationViewport* InViewportConfig)
{
	RootActor = InRootActor;
	ViewportId = InViewportId;
	ClusterNodeId = InClusterNodeId;
	ViewportConfig = InViewportConfig;

	return true;
}

UDisplayClusterDisplayDeviceBaseComponent* UDisplayClusterPreviewComponent::GetDisplayDevice() const
{
	UDisplayClusterDisplayDeviceBaseComponent* DeviceBaseComponent = nullptr;
	if (IsValid(ViewportConfig) && RootActor)
	{
		UDisplayClusterDisplayDeviceBaseComponent* CachedComponent =
			Cast<UDisplayClusterDisplayDeviceBaseComponent>(CachedDisplayDevice.GetComponent(GetOwner()));
		if (CachedComponent && CachedComponent->GetName() == ViewportConfig->DisplayDeviceName)
		{
			DeviceBaseComponent = CachedComponent;
		}
		else
		{
			DeviceBaseComponent = UE::DisplayClusterDisplayDeviceUtils::FindAndSyncDisplayDeviceFromViewport(ViewportConfig);
		}
	}

	if (bUseDisplayDevice)
	{
		CachedDisplayDevice.OverrideComponent = DeviceBaseComponent;
		CachedDisplayDevice.ComponentProperty = DeviceBaseComponent ? DeviceBaseComponent->GetFName() : NAME_None;
	}
	
	return DeviceBaseComponent;
}

bool UDisplayClusterPreviewComponent::IsPreviewEnabled() const
{
	return ViewportConfig && RootActor && RootActor->IsPreviewEnabled();
}

bool UDisplayClusterPreviewComponent::IsPreviewDrawnToScreen() const
{
	return ViewportConfig && RootActor && (RootActor->IsPreviewDrawnToScreens() || OverrideTexture);
}

void UDisplayClusterPreviewComponent::RestorePreviewMeshMaterial()
{
	UpdatePreviewMeshReference();

	if (PreviewMesh)
	{
		// Restore
		CurrentMeshMaterial = GetMeshMaterialFromDisplayDevice();
		PreviewMesh->SetMaterial(0, CurrentMeshMaterial);
		PreviewMaterialInstance = nullptr;
	}

	// Release RTTs
	// To fix UE-176749, we only want to do this if the root actor is not rendering previews, which can happen even if the previews
	// are not being output to the meshes (for example, if the ICVFX panel is open)
	// TODO: We will eventually want to separate out the preview rendering and resources from the preview mesh and material management
	if (!IsPreviewEnabled())
	{
		ReleasePreviewRenderTarget();
	}
}

void UDisplayClusterPreviewComponent::SetPreviewMeshMaterial(UMaterial* InMaterial)
{
	UpdatePreviewMeshReference();

	if (PreviewMesh)
	{
		CurrentMeshMaterial = InMaterial;

		if (InMaterial != nullptr && PreviewMaterialInstance == nullptr)
		{
			PreviewMaterialInstance = UMaterialInstanceDynamic::Create(InMaterial, this);
		}
		UpdatePreviewMaterial();

		// Set preview material
		if (PreviewMaterialInstance)
		{
			PreviewMesh->SetMaterial(0, PreviewMaterialInstance);
		}
	}
}

UMaterial* UDisplayClusterPreviewComponent::GetPreviewMaterialFromDisplayDevice() const
{
	const UDisplayClusterDisplayDeviceBaseComponent* DeviceBaseComponent = GetDisplayDevice();
	return DeviceBaseComponent ? DeviceBaseComponent->GetPreviewMaterial() : nullptr;
}

UMaterial* UDisplayClusterPreviewComponent::GetMeshMaterialFromDisplayDevice() const
{
	const UDisplayClusterDisplayDeviceBaseComponent* DeviceBaseComponent = GetDisplayDevice();
	return DeviceBaseComponent ? DeviceBaseComponent->GetMeshMaterial() : nullptr;
}

void UDisplayClusterPreviewComponent::UpdatePreviewMeshReference()
{
	if (PreviewMesh && PreviewMesh->GetName().Find(TEXT("TRASH_")) != INDEX_NONE)
	{
		// Screen components are regenerated from construction scripts, but preview components are added in dynamically. This preview component may end up
		// pointing to invalid data on reconstruction.
		// TODO: See if we can remove this hack
		ReleasePreviewMesh();
	}
}

bool UDisplayClusterPreviewComponent::UpdatePreviewMesh()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::UpdatePreviewMesh"), STAT_UpdatePreviewMesh, STATGROUP_NDisplay);

	check(IsInGameThread());

	UpdatePreviewMeshReference();

	if (IsPreviewDrawnToScreen())
	{
		check(ViewportConfig);

		// And search for new mesh reference
		IDisplayClusterViewport* Viewport = GetCurrentViewport();

		// Determine if the preview component needs to output a render or texture to the stage's screen meshes.
		// It must have a renderable resource (preview render target or override texture), have a valid viewport configured,
		// and have that viewport either actively being rendered to by the preview renderer OR have an override texture supplied
		// externally
		const bool bHasRenderableResource = RenderTarget != nullptr || RenderTargetPostProcess != nullptr || OverrideTexture != nullptr;
		const bool bIsViewportValid = Viewport != nullptr && Viewport->GetProjectionPolicy().IsValid();
		const bool bOutputToPreviewMesh = bHasRenderableResource && bIsViewportValid && (Viewport->GetRenderSettings().bEnable || OverrideTexture);
		if (bOutputToPreviewMesh)
		{
			// Handle preview mesh:
			if (Viewport->GetProjectionPolicy()->HasPreviewMesh())
			{
				// create warp mesh or update changes
				if (PreviewMesh != nullptr)
				{
					if (Viewport->GetProjectionPolicy()->IsConfigurationChanged(&WarpMeshSavedProjectionPolicy))
					{
						RestorePreviewMeshMaterial();
						ReleasePreviewMesh();
					}
				}

				if (PreviewMesh == nullptr)
				{
					// Get new mesh ptr
					PreviewMesh = Viewport->GetProjectionPolicy()->GetOrCreatePreviewMeshComponent(Viewport, bIsRootActorPreviewMesh);

					// Update saved proj policy parameters
					WarpMeshSavedProjectionPolicy = ViewportConfig->ProjectionPolicy;
				}

				// disable shadow rendering for preview meshes
				if (PreviewMesh != nullptr)
				{
					PreviewMesh->SetCastShadow(false);
				}

				UMaterial* PreviewMaterial = GetPreviewMaterialFromDisplayDevice();
				if (CurrentMeshMaterial != PreviewMaterial)
				{
					// Assign preview material to mesh
					SetPreviewMeshMaterial(PreviewMaterial);
				}

				return true;
			}

			// Policy without preview mesh
			RestorePreviewMeshMaterial();
			ReleasePreviewMesh();

			return true;
		}
	}

	// Viewport don't render
	RestorePreviewMeshMaterial();
	ReleasePreviewMesh();

	return false;
}

void UDisplayClusterPreviewComponent::ReleasePreviewMesh()
{
	// Forget old mesh with material
	PreviewMesh = nullptr;
	CurrentMeshMaterial = nullptr;
}

void UDisplayClusterPreviewComponent::UpdatePreviewResources()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::UpdatePreviewResources"), STAT_UpdatePreviewResources, STATGROUP_NDisplay);
	
	if (GetWorld())
	{
		UpdatePreviewRenderTarget();
		UpdatePreviewMesh();
	}
}

void UDisplayClusterPreviewComponent::UpdatePreviewMaterial()
{
	if (PreviewMaterialInstance != nullptr)
	{
		// TODO: Consider moving these parameter values to the display device component.
		if (OverrideTexture)
		{
			PreviewMaterialInstance->SetTextureParameterValue(TEXT("Preview"), OverrideTexture);
		}
		else
		{
			PreviewMaterialInstance->SetTextureParameterValue(TEXT("Preview"),
				RootActor && RootActor->bPreviewEnablePostProcess ? RenderTargetPostProcess : RenderTarget);
		}

		// Allow display device to perform any processing on the material instance.
		if (UDisplayClusterDisplayDeviceBaseComponent* CachedComponent =
			Cast<UDisplayClusterDisplayDeviceBaseComponent>(CachedDisplayDevice.GetComponent(GetOwner())))
		{
			CachedComponent->OnUpdatePreviewMaterialInstance(PreviewMaterialInstance);
		}
	}
}

void UDisplayClusterPreviewComponent::ReleasePreviewMaterial()
{
	if (PreviewMaterialInstance != nullptr)
	{
		// Clear the material parameters to ensure that no pointers to the texture resources are being kept around
		// Materials destroy their resources in BeginDestroy, so only clear the parameters BeginDestroy hasn't already been called
		if (!PreviewMaterialInstance->HasAnyFlags(RF_BeginDestroyed))
		{
			PreviewMaterialInstance->ClearParameterValues();
		}

		PreviewMaterialInstance = nullptr;
	}
}

void UDisplayClusterPreviewComponent::ReleasePreviewRenderTarget()
{
	ReleaseRenderTargetImpl(&RenderTarget);
	ReleaseRenderTargetImpl(&RenderTargetPostProcess);
}

void UDisplayClusterPreviewComponent::UpdatePreviewRenderTarget()
{
	if (RootActor)
	{
		if (RootActor->ShouldThisFrameOutputPreviewToPostProcessRenderTarget())
		{
			UpdateRenderTargetImpl(&RenderTargetPostProcess);
		}
		else
		{
			UpdateRenderTargetImpl(&RenderTarget);
		}
	}
}

template<typename T>
void UDisplayClusterPreviewComponent::ReleaseRenderTargetImpl(T* InOutRenderTarget)
{
	checkSlow(InOutRenderTarget);
	T& RenderTargetPtr = *InOutRenderTarget;

	if (RenderTargetPtr != nullptr)
	{
		RenderTargetPtr->ReleaseResource();
		RenderTargetPtr->MarkAsGarbage();

		RenderTargetPtr = nullptr;
	}
}

template<typename T>
void UDisplayClusterPreviewComponent::UpdateRenderTargetImpl(T* InOutRenderTarget)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UDisplayClusterPreviewComponent::UpdatePreviewRenderTarget"), STAT_UpdatePreviewRenderTarget, STATGROUP_NDisplay);
	
	EPixelFormat TextureFormat = EPixelFormat::PF_Unknown;
	FIntPoint    TextureSize(1,1);
	float        TextureGamma = 1.f;
	bool         bTextureSRGB = false;

	checkSlow(InOutRenderTarget);
	T& RenderTargetPtr = *InOutRenderTarget;
	
	if (GetPreviewTextureSettings(TextureSize, TextureFormat, TextureGamma, bTextureSRGB))
	{
		if (RenderTargetPtr != nullptr)
		{
			// Re-create RTT when format changed
			if (RenderTargetPtr->GetFormat() != TextureFormat)
			{
				ReleaseRenderTargetImpl(&RenderTargetPtr);
			}
		}

		if (RenderTargetPtr != nullptr)
		{
			// Update an existing RTT resource only when settings change
			if (RenderTargetPtr->TargetGamma != TextureGamma
				|| RenderTargetPtr->SRGB != bTextureSRGB
				|| RenderTargetPtr->GetSurfaceWidth() != TextureSize.X
				|| RenderTargetPtr->GetSurfaceHeight() != TextureSize.Y)
			{
				RenderTargetPtr->TargetGamma = TextureGamma;
				RenderTargetPtr->SRGB = bTextureSRGB;

				RenderTargetPtr->ResizeTarget(TextureSize.X, TextureSize.Y);
			}
		}
		else
		{
			// Create new RTT
			RenderTargetPtr = NewObject<UTextureRenderTarget2D>(this);
			RenderTargetPtr->ClearColor = FLinearColor::Black;
			RenderTargetPtr->TargetGamma = TextureGamma;
			RenderTargetPtr->SRGB = bTextureSRGB;

			RenderTargetPtr->InitCustomFormat(TextureSize.X, TextureSize.Y, TextureFormat, false);
			UpdatePreviewMaterial();
		}
	}
	else
	{
		//@todo: disable this viewport
		if (RenderTargetPtr)
		{
			// clear preview RTT to black in this case
			FTextureRenderTarget2DResource* TexResource = (FTextureRenderTarget2DResource*)RenderTargetPtr->GetResource();
			if (TexResource)
			{
				FCanvas Canvas(TexResource, NULL, FGameTime(), GMaxRHIFeatureLevel);
				Canvas.Clear(FLinearColor::Black);
			}
		}
	}

	*InOutRenderTarget = RenderTargetPtr;
}

bool UDisplayClusterPreviewComponent::GetPreviewTextureSettings(FIntPoint& OutSize, EPixelFormat& OutTextureFormat, float& OutGamma, bool& bOutSRGB) const
{
	if (FDisplayClusterViewport* Viewport = static_cast<FDisplayClusterViewport*>(GetCurrentViewport()))
	{
		if (FDisplayClusterViewportManager* ViewportManager = Viewport->GetViewportManagerImpl())
		{
			// The viewport size is already capped for RenderSettings
			if (!Viewport->GetContexts().IsEmpty())
			{
				FDisplayClusterViewportResourceSettings DefaultResourceSettings(ViewportManager->GetRenderFrameSettings(), nullptr);

				OutSize = Viewport->GetContexts()[0].FrameTargetRect.Size();
				OutTextureFormat = DefaultResourceSettings.GetFormat();
				OutGamma = DefaultResourceSettings.GetDisplayGamma();
				bOutSRGB = EnumHasAnyFlags(DefaultResourceSettings.GetResourceFlags(), EDisplayClusterViewportResourceSettingsFlags::ShouldUseSRGB);

				check(OutSize.X > 0);
				check(OutSize.Y > 0);

				return true;
			}
		}
	}

	return false;
}

UTexture* UDisplayClusterPreviewComponent::GetViewportPreviewTexture2D()
{
	return RenderTarget;
}

void UDisplayClusterPreviewComponent::SetOverrideTexture(UTexture* InOverrideTexture)
{
	if (OverrideTexture != InOverrideTexture)
	{
		OverrideTexture = InOverrideTexture;

		// Update the entire preview mesh here, as the preview mesh may not be configured for rendering the override texture if
		// the owning root actor has previews disabled.
		UpdatePreviewMesh();
		UpdatePreviewMaterial();
	}
}

void UDisplayClusterPreviewComponent::SetUseDisplayDevice(bool bInNewValue)
{
	bUseDisplayDevice = bInNewValue;
	CachedDisplayDevice.OverrideComponent.Reset();
	CachedDisplayDevice.ComponentProperty = NAME_None;
}

#endif
