// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandListImmediate;
class IDisplayClusterViewport;
class IDisplayClusterViewportProxy;
class IDisplayClusterWarpBlend;
class IDisplayClusterWarpPolicy;
class UMeshComponent;
class USceneComponent;
struct FDisplayClusterConfigurationProjection;
struct FMinimalViewInfo;

/**
 * nDisplay projection policy
 */
class IDisplayClusterProjectionPolicy
{
public:
	virtual ~IDisplayClusterProjectionPolicy() = default;

public:
	/**
	* Return projection policy instance ID
	*/
	virtual const FString& GetId() const = 0;

	/**
	* Return projection policy type
	*/
	virtual const FString& GetType() const = 0;

	/**
	* Return projection policy type
	*/
	UE_DEPRECATED(5.1, "This function has been deprecated. Please use 'GetType'.")
	virtual const FString GetTypeId() const
	{
		return GetType();
	}

	/**
	* Return projection policy configuration
	*/
	virtual const TMap<FString, FString>& GetParameters() const = 0;

	/**
	 * Return Origin point component used by this viewport
	 * This component is used to convert from the local DCRA space to the local projection policy space,
	 * which contains the calibrated geometry data used for the warp.
	 */
	virtual const USceneComponent* const GetOriginComponent() const
	{
		return nullptr;
	}

	/**
	* Send projection policy game thread data to render thread proxy
	* called once per frame from FDisplayClusterViewportManager::FinalizeNewFrame
	*/
	virtual void UpdateProxyData(IDisplayClusterViewport* InViewport)
	{ }

	/**
	* Called each time a new game level starts
	*
	* @param InViewport - a owner viewport
	*/
	virtual bool HandleStartScene(IDisplayClusterViewport* InViewport)
	{
		return true;
	}

	/**
	* Called when current level is going to be closed (i.e. before loading a new map)
	*
	* @param InViewport - a owner viewport
	*/
	virtual void HandleEndScene(IDisplayClusterViewport* InViewport)
	{ }

	/**
	* Set warp policy for this projection
	*
	* @param InWarpPolicy - the warp policy instance
	*/
	virtual void SetWarpPolicy(IDisplayClusterWarpPolicy* InWarpPolicy)
	{ }

	/**
	* Get warp policy
	*/
	virtual IDisplayClusterWarpPolicy* GetWarpPolicy() const
	{
		return nullptr;
	}

	/**
	* Get warp policy on rendering thread
	*/
	virtual IDisplayClusterWarpPolicy* GetWarpPolicy_RenderThread() const
	{
		return nullptr;
	}

	// Handle request for additional render targetable resource inside viewport api for projection policy
	virtual bool ShouldUseAdditionalTargetableResource() const
	{ 
		return false; 
	}

	/**
	* Returns true if the policy supports input mip-textures.
	* Use a mip texture for smoother deformation on curved surfaces.
	*
	* @return - true, if mip-texture is supported by the policy implementation
	*/
	virtual bool ShouldUseSourceTextureWithMips() const
	{
		return false;
	}

	// This policy can support ICVFX rendering
	UE_DEPRECATED(5.3, "This function has been deprecated. Please use 'ShouldSupportICVFX(IDisplayClusterViewport*)'.")
	virtual bool ShouldSupportICVFX() const
	{
		return false;
	}

	/** Returns true if this policy supports ICVFX rendering
	 * 
	 * @param InViewport - a owner viewport
	 */
	virtual bool ShouldSupportICVFX(IDisplayClusterViewport* InViewport) const
	{
		return false;
	}

	// Return true, if camera projection visible for this viewport geometry
	// ICVFX Performance : if camera frame not visible on this node, disable render for this camera
	virtual bool IsCameraProjectionVisible(const FRotator& InViewRotation, const FVector& InViewLocation, const FMatrix& InProjectionMatrix)
	{
		return true;
	}

	/**
	* Check projection policy settings changes
	*
	* @param InConfigurationProjectionPolicy - new settings
	*
	* @return - True if found changes
	*/
	virtual bool IsConfigurationChanged(const struct FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) const = 0;

	/** Override view from this projection policy
	 *
	 * @param InViewport                 - the viewport of this projection policy
	 * @param InDeltaTime                - delta time in current frame
	 * @param InOutViewInfo              - ViewInfo data
	 * @param OutCustomNearClippingPlane - Custom NCP, or a value less than zero if not defined.
	 */
	virtual void SetupProjectionViewPoint(IDisplayClusterViewport* InViewport, const float InDeltaTime, FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane = nullptr)
	{ }

	/** Projection policy can override PP */
	virtual void UpdatePostProcessSettings(IDisplayClusterViewport* InViewport)
	{ }

	/**
	* @param ViewIdx           - Index of view that is being processed for this viewport
	* @param InOutViewLocation - (in/out) View location with ViewOffset (i.e. left eye pre-computed location)
	* @param InOutViewRotation - (in/out) View rotation
	* @param ViewOffset        - Offset applied ot a camera location that gives us InOutViewLocation (i.e. right offset in world to compute right eye location)
	* @param WorldToMeters     - Current world scale (units (cm) in meter)
	* @param NCP               - Distance to the near clipping plane
	* @param FCP               - Distance to the far  clipping plane
	*
	* @return - True if success
	*/
	virtual bool CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) = 0;

	/**
	* @param ViewIdx      - Index of view that is being processed for this viewport
	* @param OutPrjMatrix - (out) projection matrix
	*
	* @return - True if success
	*/
	virtual bool GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix) = 0;

	/**
	* Returns if a policy provides warp&blend feature
	*
	* @return - True if warp&blend operations are supported by the policy implementation
	*/
	virtual bool IsWarpBlendSupported()
	{
		return false;
	}

	/**
	* Initializing the projection policy logic for the current frame before applying warp blending. Called if IsWarpBlendSupported() returns true
	*
	* @param RHICmdList      - RHI commands
	* @param InViewportProxy - viewport proxy
	*
	*/
	virtual void BeginWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
	{ }

	/**
	* Performs warp&blend. Called if IsWarpBlendSupported() returns true
	*
	* @param RHICmdList      - RHI commands
	* @param InViewportProxy - viewport proxy
	*
	*/
	virtual void ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
	{ }

	/**
	* Completing the projection policy logic for the current frame after applying warp blending. Called if IsWarpBlendSupported() returns true
	*
	* @param ViewIdx      - Index of view that is being processed for this viewport
	* @param RHICmdList   - RHI commands
	* @param SrcTexture   - Source texture
	* @param ViewportRect - Region of the SrcTexture to perform warp&blend operations
	*
	*/
	virtual void EndWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
	{ }

	/**
	* Return warpblend interface of this policy
	*
	* @param OutWarpBlendInterface - output interface ref
	* 
	* @return - true if output interface valid
	*/
	virtual bool GetWarpBlendInterface(TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlendInterface) const
	{
		return false;
	}

	/**
	* Return warpblend interface proxy of this policy
	*
	* @param OutWarpBlendInterfaceProxy - output interface ref
	*
	* @return - true if output interface valid
	*/
	virtual bool GetWarpBlendInterface_RenderThread(TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe>& OutWarpBlendInterfaceProxy) const
	{
		return false;
	}

#if WITH_EDITOR
	/**
	* Ask projection policy instance if it has any mesh based preview
	*
	* @return - True if mesh based preview is available
	*/
	virtual bool HasPreviewMesh()
	{
		return false;
	}

	/**
	* Build preview mesh
	* This MeshComponent cannot be moved freely.
	* This MeshComponent is attached to the geometry from the real world via Origin.
	* When the screen geometry in the real world changes position, the position of this component must also be changed.
	*
	* @param InViewport - Projection specific parameters.
	* @param bOutIsRootActorComponent - return true, if used custom root actor component. return false, if created unique temporary component
	*/
	virtual UMeshComponent* GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent)
	{
		return nullptr;
	}

	/**
	* Ask projection policy instance if it has any movable mesh based preview
	*
	* @return - True if mesh based preview is available
	*/
	virtual bool HasPreviewMovableMesh()
	{
		return false;
	}

	/**
	* Build preview movable mesh
	* This MeshComponent is a copy of the preview mesh and can be moved freely with the UI visualization.
	*
	* @param InViewport - Projection specific parameters.
	*/
	virtual UMeshComponent* GetOrCreatePreviewMovableMeshComponent(IDisplayClusterViewport* InViewport)
	{
		return nullptr;
	}
#endif
};
