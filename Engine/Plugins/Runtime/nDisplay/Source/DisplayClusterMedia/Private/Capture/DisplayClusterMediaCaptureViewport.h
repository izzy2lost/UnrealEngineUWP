// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Capture/DisplayClusterMediaCaptureBase.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

class FRDGBuilder;
class FSceneViewFamily;
class IDisplayClusterViewport;
class IDisplayClusterViewportProxy;


/**
 * Viewport media capture
 */
class FDisplayClusterMediaCaptureViewport
	: public FDisplayClusterMediaCaptureBase
{
public:
	FDisplayClusterMediaCaptureViewport(const FString& MediaId, const FString& ClusterNodeId, const FString& ViewportId, UMediaOutput* MediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr);

public:
	virtual bool StartCapture() override;
	virtual void StopCapture() override;

	const FString& GetViewportId() const
	{
		return ViewportId;
	}

protected:
	
	virtual FIntPoint GetCaptureSize() const override;

private:
	void OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const IDisplayClusterViewportProxy* ViewportProxy);

	/** Media is completely independent of DC viewports. In addition, multiple media can use the same viewport in different ways.
	* The viewport does not know about the configuration of the media. But the viewport needs some special media flags that change its internal logic.
	* To get these flags, the viewport uses a multicast delegate to get all these flags from multiple media.
	* On the media side, they must subscribe to this callback and raise these flags to change the viewport logic in the expected way.
	*/
	void OnUpdateViewportMediaState(IDisplayClusterViewport* InViewport, EDisplayClusterViewportMediaState& InOutMediaState);

public:
	// Note: viewport is unaware of the media's configurations.
	// Therefore, any future changes to the USTRUCT used by media do not affect the logic in the DisplayCluster module.
	// These flags are passed to the viewport in the OnUpdateViewportMediaState() callback function.

	// Force late OCIO pass
	bool ForceLateOCIOPass = false;

private:
	const FString ViewportId;
};
