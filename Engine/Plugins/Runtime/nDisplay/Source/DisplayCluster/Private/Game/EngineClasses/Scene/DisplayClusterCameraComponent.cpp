// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterCameraComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/Misc/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Postprocess.h"

#include "DisplayClusterRootActor.h"

#include "Engine/CollisionProfile.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

#include "DisplayClusterConfigurationTypes.h"


UDisplayClusterCameraComponent::UDisplayClusterCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, bEnableGizmo(true)
	, BaseGizmoScale(0.5f, 0.5f, 0.5f)
	, GizmoScaleMultiplier(1.f)
#endif
	, InterpupillaryDistance(6.4f)
	, bSwapEyes(false)
	, StereoOffset(EDisplayClusterEyeStereoOffset::None)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTextureObject = TEXT("/nDisplay/Icons/S_nDisplayViewOrigin");
		SpriteTexture = SpriteTextureObject.Get();
	}
#endif
}

void UDisplayClusterCameraComponent::ApplyViewPointComponentPostProcessesToViewport(IDisplayClusterViewport* InViewport)
{
	check(InViewport && !EnumHasAnyFlags(InViewport->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource));

	// Viewports that use the ViewPoint component get post - processing and more from the referenced camera component.
	// As we can see, we will have to use up to 3 different classes as different sources of these settings :
	// UCameraComponent->UCineCameraComponent->UDisplayClusterICVFXCameraComponent
	// Thus, we have to use all available rendering settings in our component's class.
	// Override viewport PP from camera, except internal ICVFX viewports
		
	using namespace UE::DisplayClusterViewportHelpers;
	// Get the same component from DCRA that is used as the configuration source. Then this component can also be used as a configuration data source.
	const UDisplayClusterCameraComponent& CfgCameraComponent = GetMatchingComponentFromRootActor(InViewport->GetConfiguration(), EDisplayClusterRootActorType::Configuration, *this);

	// Setup Outer Viewport postprocessing
	if (CfgCameraComponent.bEnableOuterViewportCamera)
	{
		const EDisplayClusterViewportCameraPostProcessFlags CameraPostProcessingFlags = CfgCameraComponent.OuterViewportPostProcessSettings.GetCameraPostProcessFlags();

		// Also, if we are referencing the ICVFXCamera component, use the special ICVFX PostProcess from it.
		if (UDisplayClusterICVFXCameraComponent* SceneICVFXCameraComponent = GetRootActorComponentByName<UDisplayClusterICVFXCameraComponent>(InViewport->GetConfiguration(), EDisplayClusterRootActorType::Scene, CfgCameraComponent.OuterViewportCameraName))
		{
			// Use PostProcess from the ICVFXCamera
			// This function also uses PostProcess from the parent CineCamera class.
			SceneICVFXCameraComponent->ApplyICVFXCameraPostProcessesToViewport(InViewport, CameraPostProcessingFlags);
		}
		else
		{
			// Use post-processing settings from Camera/CineCamera or from the active game camera.
			FMinimalViewInfo CustomViewInfo;
			if (CfgCameraComponent.GetOuterViewportCameraDesiredViewInternal(InViewport->GetConfiguration(), CustomViewInfo))
			{
				// Applies a filter to the post-processing settings.
				FDisplayClusterViewportConfigurationHelpers_Postprocess::FilterPostProcessSettings(CustomViewInfo.PostProcessSettings, CameraPostProcessingFlags);

				// Send camera postprocess to override
				InViewport->GetViewport_CustomPostProcessSettings().AddCustomPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override, CustomViewInfo.PostProcessSettings, CustomViewInfo.PostProcessBlendWeight, true);
			}
		}
	}
}

UCameraComponent* UDisplayClusterCameraComponent::GetOuterViewportCameraComponent(const IDisplayClusterViewportConfiguration& InViewportConfiguration) const
{
	using namespace UE::DisplayClusterViewportHelpers;

	// Get the same component from DCRA that is used as the configuration source. Then this component can also be used as a configuration data source.
	const UDisplayClusterCameraComponent& CfgCameraComponent = GetMatchingComponentFromRootActor(InViewportConfiguration, EDisplayClusterRootActorType::Configuration, *this);
	if (CfgCameraComponent.bEnableOuterViewportCamera)
	{
		if(UCameraComponent* SceneCameraComponent = GetRootActorComponentByName<UCameraComponent>(InViewportConfiguration, EDisplayClusterRootActorType::Scene, CfgCameraComponent.OuterViewportCameraName))
		{
			// If we use the ICVFX camera component, we must use GetActualCineCameraComponent() to get the actual camera.
			if(SceneCameraComponent->IsA<UDisplayClusterICVFXCameraComponent>())
			{
				if (UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent = Cast<UDisplayClusterICVFXCameraComponent>(SceneCameraComponent))
				{
					if (UCineCameraComponent* ExtCineCameraComponent = ICVFXCameraComponent->GetActualCineCameraComponent())
					{
						// Use referenced camera as the source of Camera PP and CineCamera CustomNearClippingPlane
						return ExtCineCameraComponent;
					}
					}
				}

			return SceneCameraComponent;
		}
	}

	return nullptr;
}

bool UDisplayClusterCameraComponent::GetOuterViewportCameraDesiredViewInternal(const IDisplayClusterViewportConfiguration& InViewportConfiguration, FMinimalViewInfo& InOutViewInfo, float* OutCustomNCP) const
{
	using namespace UE::DisplayClusterViewportHelpers;

	bool bViewInfoFound = false;

	// Get the same component from DCRA that is used as the configuration source. Then this component can also be used as a configuration data source.
	const UDisplayClusterCameraComponent& CfgCameraComponent = GetMatchingComponentFromRootActor(InViewportConfiguration, EDisplayClusterRootActorType::Configuration, *this);
	if (CfgCameraComponent.bEnableOuterViewportCamera)
	{
		const EDisplayClusterViewportCameraPostProcessFlags CameraPostProcessingFlags = CfgCameraComponent.OuterViewportPostProcessSettings.GetCameraPostProcessFlags();

		const bool bUseCameraPostprocess = EnumHasAnyFlags(CameraPostProcessingFlags, EDisplayClusterViewportCameraPostProcessFlags::EnablePostProcess);

		float* OutCustomNearClippingPlane = OutCustomNCP;
		if (!EnumHasAnyFlags(CameraPostProcessingFlags, EDisplayClusterViewportCameraPostProcessFlags::EnableNearClippingPlane))
		{
			// Ignore NCP from the custom camera.
			OutCustomNearClippingPlane = nullptr;
		}

		if (UCameraComponent* SceneCameraComponent = GetOuterViewportCameraComponent(InViewportConfiguration))
		{
			if (IDisplayClusterViewport::GetCameraComponentView(SceneCameraComponent, InViewportConfiguration.GetRootActorWorldDeltaSeconds(), bUseCameraPostprocess, InOutViewInfo, OutCustomNearClippingPlane))
			{
				bViewInfoFound = true;
			}
		}
		// Get PostProcess from the Game camera.
		else if (IDisplayClusterViewport::GetPlayerCameraView(InViewportConfiguration.GetCurrentWorld(), bUseCameraPostprocess, InOutViewInfo))
		{
			
			bViewInfoFound = true;
		}

		if (bViewInfoFound && !CfgCameraComponent.bFollowOuterViewportCamera)
		{
			// Use this component as a camera
			InOutViewInfo.Location = GetComponentLocation();
			InOutViewInfo.Rotation = GetComponentRotation();
		}

		// The default camera is not found, so we can't use the custom camera view.
	}

	return bViewInfoFound;
}

void UDisplayClusterCameraComponent::GetDesiredView(IDisplayClusterViewportConfiguration& InViewportConfiguration, FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane)
{
	if (GetOuterViewportCameraDesiredViewInternal(InViewportConfiguration, InOutViewInfo, OutCustomNearClippingPlane))
	{
		return;
	}

	// Ignore PP, because this component has no such settings
	InOutViewInfo.PostProcessBlendWeight = 0.f;

	if (OutCustomNearClippingPlane)
	{
		// Value less than zero means: don't override the NCP value
		*OutCustomNearClippingPlane = -1.f;
	}

	// By default this component is used as ViewPoint
	// Use this component as a camera
	InOutViewInfo.Location = GetComponentLocation();
	InOutViewInfo.Rotation = GetComponentRotation();
}

void UDisplayClusterCameraComponent::GetEyePosition(const IDisplayClusterViewportConfiguration& InViewportConfiguration, FVector& OutViewLocation, FRotator& OutViewRotation)
{
	FMinimalViewInfo ViewInfo;
	if (GetOuterViewportCameraDesiredViewInternal(InViewportConfiguration, ViewInfo))
	{
		OutViewLocation = ViewInfo.Location;
		OutViewRotation = ViewInfo.Rotation;
	}
	else
	{
		// By default this component is used as ViewPoint
		// Use this component as a camera
		OutViewLocation = GetComponentLocation();
		OutViewRotation = GetComponentRotation();
	}
}

#if WITH_EDITOR
void UDisplayClusterCameraComponent::SetVisualizationScale(float Scale)
{
	GizmoScaleMultiplier = Scale;
	RefreshVisualRepresentation();
}

void UDisplayClusterCameraComponent::SetVisualizationEnabled(bool bEnabled)
{
	bEnableGizmo = bEnabled;
	RefreshVisualRepresentation();
}
#endif

void UDisplayClusterCameraComponent::OnRegister()
{
#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		if (SpriteComponent == nullptr)
		{
			SpriteComponent = NewObject<UBillboardComponent>(this, NAME_None, RF_Transactional | RF_TextExportTransient);
			if (SpriteComponent)
			{
				SpriteComponent->SetupAttachment(this);
				SpriteComponent->SetIsVisualizationComponent(true);
				SpriteComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
				SpriteComponent->SetMobility(EComponentMobility::Movable);
				SpriteComponent->Sprite = SpriteTexture;
				SpriteComponent->SpriteInfo.Category = TEXT("NDisplayViewOrigin");
				SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("DisplayClusterCameraComponent", "NDisplayViewOriginSpriteInfo", "nDisplay View Origin");
				SpriteComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
				SpriteComponent->bHiddenInGame = true;
				SpriteComponent->bIsScreenSizeScaled = true;
				SpriteComponent->CastShadow = false;
				SpriteComponent->CreationMethod = CreationMethod;
				SpriteComponent->RegisterComponentWithWorld(GetWorld());
			}
		}
	}

	RefreshVisualRepresentation();
#endif

	Super::OnRegister();
}

#if WITH_EDITOR
void UDisplayClusterCameraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	RefreshVisualRepresentation();
}

void UDisplayClusterCameraComponent::RefreshVisualRepresentation()
{
	// Update the viz component
	if (SpriteComponent)
	{
		SpriteComponent->SetVisibility(bEnableGizmo);
		SpriteComponent->SetWorldScale3D(BaseGizmoScale * GizmoScaleMultiplier);
		// The sprite components don't get updated in real time without forcing render state dirty
		SpriteComponent->MarkRenderStateDirty();
	}
}
#endif

EDisplayClusterViewportCameraPostProcessFlags FDisplayClusterCameraComponent_OuterViewportPostProcessSettings::GetCameraPostProcessFlags() const
{
	EDisplayClusterViewportCameraPostProcessFlags OutPostProcessFlags = EDisplayClusterViewportCameraPostProcessFlags::None;

	if (bEnablePostProcess)
	{
		EnumAddFlags(OutPostProcessFlags, EDisplayClusterViewportCameraPostProcessFlags::EnablePostProcess);
	}

	if (bEnableDepthOfField)
	{
		EnumAddFlags(OutPostProcessFlags, EDisplayClusterViewportCameraPostProcessFlags::EnableDepthOfField);
	}

	if (bEnableNearClippingPlane)
	{
		EnumAddFlags(OutPostProcessFlags, EDisplayClusterViewportCameraPostProcessFlags::EnableNearClippingPlane);
	}

	if (bEnableICVFXColorGrading)
	{
		EnumAddFlags(OutPostProcessFlags, EDisplayClusterViewportCameraPostProcessFlags::EnableICVFXColorGrading);
	}

	if (bEnableICVFXMotionBlur)
	{
		EnumAddFlags(OutPostProcessFlags, EDisplayClusterViewportCameraPostProcessFlags::EnableICVFXMotionBlur);
	}

	if (bEnableICVFXDepthOfFieldCompensation)
	{
		EnumAddFlags(OutPostProcessFlags, EDisplayClusterViewportCameraPostProcessFlags::EnableICVFXDepthOfFieldCompensation);
	}

	return OutPostProcessFlags;
}

