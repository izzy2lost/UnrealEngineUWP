// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SceneViewExtension.h"

class FDisplayClusterViewportManager;

#define DISPLAYCLUSTER_SCENE_DEBUG_VIEW_EXTENSION_PRIORITY 999

/**
 * DC-specific view extension to display frame stats
 */
class FDisplayClusterViewportFrameStatsViewExtension : public FSceneViewExtensionBase
{
public:
	FDisplayClusterViewportFrameStatsViewExtension(const FAutoRegister& AutoRegister, const FDisplayClusterViewportManager* InViewportManager);
	virtual ~FDisplayClusterViewportFrameStatsViewExtension();

public:
	//~ Begin ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override { }
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override { }
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;

	void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled);
	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);

	virtual int32 GetPriority() const override { return DISPLAYCLUSTER_SCENE_DEBUG_VIEW_EXTENSION_PRIORITY; }

protected:
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	/** Get viewport manager ptr. */
	inline const FDisplayClusterViewportManager* GetViewportManager() const
	{
		return ViewportManagerWeakPtr.IsValid() ? ViewportManagerWeakPtr.Pin().Get() : nullptr;
	}

	/** True, if VE can be used at the moment. */
	bool IsActive() const;

private:
	std::atomic<uint32> FrameCount;
	std::atomic<uint32> EncodedTimecode;

	TWeakPtr<const FDisplayClusterViewportManager, ESPMode::ThreadSafe> ViewportManagerWeakPtr;
};
