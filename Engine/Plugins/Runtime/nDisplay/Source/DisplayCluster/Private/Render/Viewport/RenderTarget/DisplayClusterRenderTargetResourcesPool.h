// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/Resource/DisplayClusterViewportResource.h"

class FDisplayClusterViewportManagerProxy;
struct FDisplayClusterRenderFrameSettings;

/**
 * DC viewport resources pool
 */
class FDisplayClusterRenderTargetResourcesPool
{
public:
	FDisplayClusterRenderTargetResourcesPool(FDisplayClusterViewportManagerProxy* InViewportManagerProxy);
	~FDisplayClusterRenderTargetResourcesPool();

	void Release();

public:
	/** Begin reallocate resources for the specified cluster node.
	* 
	* @param InRenderFrameSettings - default resource settings
	* @param InViewport - the window that uses these resources
	* 
	* @return true, if success
	*/
	bool BeginReallocateResources(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, class FViewport* InViewport);
	
	/** Allocate a new resource or reuse exists
	* 
	* @param InSize            - resource dimensions
	* @param CustomPixelFormat - resource pixel format
	* @param InResourceFlags   - These flags define the behavior of the resource.
	* @param NumMips           - number of mips in the texture
	* 
	* @return ref to the resource instance
	*/
	TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> AllocateResource(const FIntPoint& InSize, EPixelFormat CustomPixelFormat, const EDisplayClusterViewportResourceSettingsFlags InResourceFlags, int32 NumMips = 1);

	/** End reallocate resources for the specified cluster node. */
	void EndReallocateResources();

private:
	/**
	 * Resource update mode
	 */
	enum class EResourceUpdateMode : uint8
	{
		Initialize = 0,
		Release
	};

	/** Update resources from array
	* 
	* @param InOutViewportResources - resources to update
	* @param InUpdateMode           - initialize or release
	*/
	void ImplUpdateResources(TArray<TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>>& InOutViewportResources, const EResourceUpdateMode InUpdateMode);

	/** Retrieving a ViewportManager instance. */
	inline FDisplayClusterViewportManagerProxy* GetViewportManagerProxy() const
	{
		return ViewportManagerProxyWeakPtr.IsValid() ? ViewportManagerProxyWeakPtr.Pin().Get() : nullptr;
	}

private:
	// Current render resource settings
	FDisplayClusterViewportResourceSettings* ResourceSettings = nullptr;

	// Viewport render resources
	TArray<TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>> ViewportResources;

	// Weak ref to the viewport manager
	TWeakPtr<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> ViewportManagerProxyWeakPtr;
};
