// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "Components/DisplayClusterCameraComponent.h"
#include "Blueprints/DisplayClusterWarpBlueprint_Enums.h"

#include "DisplayClusterInFrustumFitCameraComponent.generated.h"

class ACineCameraActor;
class UCameraComponent;
class IDisplayClusterWarpPolicy;

/**
 * 3D point in space used to project the camera view onto a group of nDisplay viewports.
 * Support projection policies: mpcdi/pfm 2d/a3d, mesh.
 */
UCLASS(ClassGroup = (DisplayCluster), meta = (BlueprintSpawnableComponent, DisplayName = "nDisplay InFrustumFit View Origin"))
class DISPLAYCLUSTERWARP_API UDisplayClusterInFrustumFitCameraComponent : public UDisplayClusterCameraComponent
{
	GENERATED_BODY()

public:
	UDisplayClusterInFrustumFitCameraComponent(const FObjectInitializer& ObjectInitializer);

	//~Begin UActorComponent
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~~~End UActorComponent

	//~Begin UDisplayClusterCameraComponent
	virtual void GetDesiredView(FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane = nullptr) override;
	virtual bool ShouldUseEntireClusterViewports(IDisplayClusterViewportManager* InViewportManager) const override;
	virtual IDisplayClusterWarpPolicy* GetWarpPolicy(IDisplayClusterViewportManager* InViewportManager) override;
	//~~End UDisplayClusterCameraComponent

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut) override;
	virtual TSharedPtr<SWidget> GetCustomEditorPreviewWidget() override;

	void InvalidatePreviewState() { bRefreshPreviewState = true; }
#endif

public:
	/** true, if camera projection is used. */
	bool IsEnabled() const;

	/** Return external camera component. */
	UCameraComponent* GetExternalCameraComponent() const;

public:
	/** Camera projection mode is used. */
	UPROPERTY(EditAnywhere, Category = "Projection")
	bool bEnableCameraProjection = true;

	/** Enable special rendering mode for all viewports using this viewpoint. */
	UPROPERTY(EditAnywhere, Category = "Projection", meta = (EditCondition = "bEnableCameraProjection"))
	EDisplayClusterWarpCameraProjectionMode CameraProjectionMode = EDisplayClusterWarpCameraProjectionMode::Fit;

	/** Indicates which camera facing mode is used when frustum fitting the stage geometry */
	UPROPERTY(EditAnywhere, Category = "Projection", meta = (EditCondition = "bEnableCameraProjection"))
	EDisplayClusterWarpCameraViewTarget CameraViewTarget = EDisplayClusterWarpCameraViewTarget::GeometricCenter;

	/** Use a specific actor camera instead of a game camera. */
	UPROPERTY(EditAnywhere, Category = "Projection", meta = (EditCondition = "bEnableCameraProjection"))
	TSoftObjectPtr<ACineCameraActor> ExternalCameraActor;

	/** Allows you to override the PostProcess of the viewport from the camera. */
	UPROPERTY(EditAnywhere, Category = "Projection", meta = (EditCondition = "bEnableCameraProjection"))
	bool bUseCameraPostprocess = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Projection", meta = (EditCondition = "bEnableCameraProjection"))
	bool bShowPreviewFrustumFit = false;
#endif

private:
	// a unique type of warp policy for this component
	// this policy class knows the properties of the component and implements the corresponding logic
	TSharedPtr<IDisplayClusterWarpPolicy, ESPMode::ThreadSafe> WarpPolicy;

#if WITH_EDITOR
	/** Indicates that the state of the preview meshes for the frustum fit are invalid and need to be refreshed on the next tick */
	bool bRefreshPreviewState = true;
#endif
};
