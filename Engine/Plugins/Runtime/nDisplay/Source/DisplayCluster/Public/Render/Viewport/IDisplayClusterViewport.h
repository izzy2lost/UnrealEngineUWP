// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Viewport/IDisplayClusterViewport_CustomPostProcessSettings.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettingsICVFX.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"

#include "Engine/EngineTypes.h"

class UCameraComponent;
class UWorld;
struct FMinimalViewInfo;

/**
 * nDisplay: Viewport (interface for GameThread)
 */
class DISPLAYCLUSTER_API IDisplayClusterViewport
{
public:
	virtual ~IDisplayClusterViewport() = default;

public:
	/** Get TSharedPtr from self. */
	virtual TSharedPtr<IDisplayClusterViewport, ESPMode::ThreadSafe> ToSharedPtr() = 0;
	virtual TSharedPtr<const IDisplayClusterViewport, ESPMode::ThreadSafe> ToSharedPtr() const = 0;

	virtual FString GetId() const = 0;
	virtual FString GetClusterNodeId() const = 0;

	virtual const FDisplayClusterViewport_RenderSettings&      GetRenderSettings() const = 0;
	virtual const FDisplayClusterViewport_RenderSettingsICVFX& GetRenderSettingsICVFX() const = 0;
	virtual const FDisplayClusterViewport_PostRenderSettings&  GetPostRenderSettings() const = 0;

	/** Calculate projection matrices for viewport context
	 * (also created overscan matrices, etc)
	 * 
	 * @param InContextNum   - viewport eye context index
	 * @param Left           - frustum left angle
	 * @param Right          - frustum right angle
	 * @param Top            - frustum top angle
	 * @param Bottom         - frustum bottom angle
	 * @param NCP            - Near clipping plane
	 * @param FCP            - Far  clipping plane
	 * @param bIsAnglesInput - true, if projection angles in degrees
	 */
	virtual void CalculateProjectionMatrix(const uint32 InContextNum, float Left, float Right, float Top, float Bottom, float ZNear, float ZFar, bool bIsAnglesInput) = 0;

	/** Calculate viewport projection (This function has been deprecated.)
	 *
	 * @param InContextNum      - viewport eye context index
	 * @param InOutViewLocation - (in, out) View location
	 * @param InOutViewRotation - (in,out) View rotator
	 * @param ViewOffset         - eye offset from view location
	 * @param WorldToMeters     - UE world scale
	 * @param NCP - Near clipping plane
	 * @param FCP - Far clipping plane
	 *
	 * @return - true, if the calculation of the viewport projection is successful.
	 */
	UE_DEPRECATED(5.3, "This function has been deprecated. Please use 'CalculateView()'.")
	virtual bool CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP)
	{
		return false;
	}

	/** Calculate viewport projection
	 * 
	 * @param InContextNum      - viewport eye context index
	 * @param InOutViewLocation - (in, out) View location
	 * @param InOutViewRotation - (in,out) View rotator
	 * @param WorldToMeters     - UE world scale
	 * 
	 * @return - true, if the calculation of the viewport projection is successful.
	 */
	virtual bool CalculateView(const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const float WorldToMeters) = 0;


	/** Get projection matrix for viewport context
	 * 
	 * @param InContextNum - viewport eye context index
	 * @param OutPrjMatrix - (out) Projection  matrix
	 * 
	 * @return true, if projection matrix valid.
	 */
	virtual bool GetProjectionMatrix(const uint32 InContextNum, FMatrix& OutPrjMatrix) = 0;

	/** Setup viewpoint for this viewport
	 *
	 * @param InOutViewInfo - [in\out] viewinfo
	 * 
	 * @return - true if there is an internal viewpoint for the given viewport.
	 */
	virtual bool SetupViewPoint(struct FMinimalViewInfo& InOutViewInfo) = 0;

	/** Return view point camera component for this viewport. */
	virtual class UDisplayClusterCameraComponent* GetViewPointCameraComponent() const = 0;

	/** Retrieves the view position from the ViewPoint component used by this viewport.
	 * The CalculateView() function receives arguments that may not match this position because they can be overridden.
	 * Therefore, projection policies that expect values from the ViewPoint component must use this function to get the correct values.
	 */
	virtual bool GetViewPointCameraEye(const uint32 InContextNum, FVector& OutViewLocation, FRotator& OutViewRotation, FVector& OutViewOffset) = 0;

	/** Get the distance from the eye to the viewpoint location.
	 *
	 * @param InContextNum - eye context of this viewport
	 */
	virtual float GetStereoEyeOffsetDistance(const uint32 InContextNum) = 0;

	virtual const TSharedPtr<class IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& GetProjectionPolicy() const = 0;

	virtual const TArray<FDisplayClusterViewport_Context>& GetContexts() const = 0;

	/** Get clipping planes used by this viewport
	 * 
	 * @return Clipping planes as FVector2D(ZNear, ZFar)
	 */
	virtual FVector2D GetClippingPlanes() const = 0;

	// Override postprocess settings for this viewport
	virtual const IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() const = 0;
	virtual IDisplayClusterViewport_CustomPostProcessSettings& GetViewport_CustomPostProcessSettings() = 0;

	// Setup scene view for rendering specified Context
	virtual void SetupSceneView(uint32 ContextNum, class UWorld* World, class FSceneViewFamily& InViewFamily, FSceneView& InView) const = 0;

	UE_DEPRECATED(5.3, "This function has beend deprecate. Please use 'GetViewportManager'.")
	virtual class IDisplayClusterViewportManager& GetOwner() const
	{
		check(false);

		return *GetViewportManager();
	}

	/** Return the viewport manager that owns this viewport */
	virtual class IDisplayClusterViewportManager* GetViewportManager() const = 0;

	/** Return the DCRA that owns this viewport */
	virtual class ADisplayClusterRootActor* GetRootActor() const = 0;

	/** Return current render mode. */
	virtual EDisplayClusterRenderFrameMode GetRenderMode() const = 0;

	/** Return current world. */
	virtual class UWorld* GetCurrentWorld() const = 0;

	/** Returns true if the scene is open now (The current world is assigned and DCRA has already initialized for it). */
	virtual bool IsSceneOpened() const = 0;

	/** Returns true if the current world type is equal to one of the input types. */
	virtual bool IsCurrentWorldHasAnyType(const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2 = EWorldType::None, const EWorldType::Type InWorldType3 = EWorldType::None) const = 0;

	virtual void SetRenderSettings(const FDisplayClusterViewport_RenderSettings& InRenderSettings) = 0;
	virtual void SetContexts(TArray<FDisplayClusterViewport_Context>& InContexts) = 0;

	static FMatrix MakeProjectionMatrix(float InLeft, float InRight, float InTop, float InBottom, float ZNear, float ZFar);

	/** Get View from CameraComponent
	*
	* @param InCameraComponent          - Ptr to the camera component used for rendering. if this ptr is nullptr, then the active player camera is used
	* @param InDeltaTime                - delta time in current frame
	* @param bUseCameraPostprocess      - If false, then InOutViewInfo.PostProcessBlendWeight is set to 0.
	* @param InOutViewInfo              - ViewPoint data
	* @param OutCustomNearClippingPlane - Custom NCP, or a value less than zero if not defined.
	*
	* @return false if the viewpoint data cannot be retrieved.
	*/
	static bool GetCameraComponentView(UCameraComponent* InCameraComponent, const float InDeltaTime, const bool bUseCameraPostprocess, FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane = nullptr);

	/** Get View from current player camera
	*
	* @param InWorld - In this world, an active game camera will be searched for.
	* @param bUseCameraPostprocess - if false, then InOutViewInfo.PostProcessBlendWeight is set to 0.
	* @param InOutViewInfo         - (in,out) ViewPoint data
	*
	* @return false if the viewpoint data cannot be retrieved.
	*/
	static bool GetPlayerCameraView(UWorld* InWorld, const bool bUseCameraPostprocess, FMinimalViewInfo& InOutViewInfo);
};
