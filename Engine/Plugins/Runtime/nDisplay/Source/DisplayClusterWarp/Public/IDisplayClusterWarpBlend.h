// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Containers/DisplayClusterWarpContext.h"
#include "Containers/DisplayClusterWarpEye.h"

class UMeshComponent;
/**
 * WarpBlend interface for MPCDI and mesh projection policies
 */
class IDisplayClusterWarpBlend
{
public:
	virtual ~IDisplayClusterWarpBlend() = default;

public:
	/** Get TSharedPtr from self. */
	virtual TSharedPtr<IDisplayClusterWarpBlend, ESPMode::ThreadSafe> ToSharedPtr() = 0;
	virtual TSharedPtr<const IDisplayClusterWarpBlend, ESPMode::ThreadSafe> ToSharedPtr() const = 0;

	/** Update internal geometry cached data
	 */
	virtual bool UpdateGeometryContext(const float InWorldScale) = 0;

	/** Get geometry context data. */
	virtual const FDisplayClusterWarpGeometryContext& GetGeometryContext() const = 0;

	/**
	* Calculate warp context data for new eye
	*
	* @param InEye - Current eye and scene
	*
	* @return - true if the context calculated successfully
	*/
	virtual bool CalcFrustumContext(const TSharedPtr<FDisplayClusterWarpEye, ESPMode::ThreadSafe>& InWarpEye) = 0;

	/**
	 * Access to resources by type
	 */
	virtual class FRHITexture* GetTexture(EDisplayClusterWarpBlendTextureType InTextureType) const = 0;

	/**
	 * Return AlphaMap embedded gamma value
	 */
	virtual float GetAlphaMapEmbeddedGamma() const = 0;

	/**
	 * Get mesh proxy
	 */
	virtual const class IDisplayClusterRender_MeshComponentProxy* GetWarpMeshProxy_RenderThread() const = 0;

	/**
	 * MPCDI profile type
	 */
	virtual EDisplayClusterWarpProfileType  GetWarpProfileType() const = 0;

	/**
	 * Get MPCDI attributes (or their default values if they are not defined)
	 */
	virtual const FDisplayClusterWarpMPCDIAttributes& GetMPCDIAttributes() const = 0;

	/**
	 * Warp geometry type
	 */
	virtual EDisplayClusterWarpGeometryType GetWarpGeometryType() const = 0;

	/**
	 * Warp frustum geometry type
	 */
	virtual EDisplayClusterWarpFrustumGeometryType GetWarpFrustumGeometryType() const = 0;

	/**
	 * Export warp geometry to memory
	 */
	virtual bool ExportWarpMapGeometry(struct FDisplayClusterWarpGeometryOBJ& OutMeshData, uint32 InMaxDimension = 0) const = 0;

	/**
	* Mark internal component ref as dirty for geometry update
	*
	* @param InComponentName - (optional) the name of the internal geometry ref component. Empty string for any component name
	*
	* @return - true, if there is a marked.
	*/
	virtual bool MarkWarpGeometryComponentDirty(const FName& InComponentName) = 0;

	/** Return true, if this WarpBlend support ICVFX pipeline. */
	virtual bool ShouldSupportICVFX() const = 0;

	/** Get internal warp context data. */
	virtual FDisplayClusterWarpData& GetWarpData(const uint32 ContextNum) = 0;

	/** Get internal warp context data. */
	virtual const FDisplayClusterWarpData& GetWarpData(const uint32 ContextNum) const = 0;

	/** Get frustum mesh component. */
	virtual UMeshComponent* GetStaticMeshComponent() const = 0;
};
