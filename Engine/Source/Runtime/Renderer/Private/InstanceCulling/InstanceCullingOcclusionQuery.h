// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"
#include "RenderGraphResources.h"
#include "Containers/Array.h"

class FRDGBuilder;
class FGPUScene;
class FViewInfo;
struct IPooledRenderTarget;

/** 
* Implements accurate per-instance occlusion culling similar to FOcclusionFeedback, except using GPUScene.
*/
class FInstanceCullingOcclusionQueryRenderer
{
public:

	/**
	* Perform per-instance occlusion culling using HZB and software occlusion queries.
	* Returns view-specific bit mask that should be used when interpreting query result buffer, since it contains 1 bit per view.
	*/	
	uint32 Render(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene, FViewInfo& View);

	/**
	* Perform various book-keeping tasks that must run at the end of each frame
	* Saves view ID mapping data, extracts result buffer to be available next frame, etc.
	*/
	void EndFrame(FRDGBuilder& GraphBuilder);

	/**
	* One uint32 per instance in GPUScene, with 1 bit per view (see GetOcclusionQueryMaskForView)
	* Available when when instance culling is enabled and r.InstanceCulling.OcclusionQueries=1.
	* Contains data for *previous* frame. Assumes that GPUScene instance indices are consistent.
	*/
	TRefCountPtr<FRDGPooledBuffer> InstanceOcclusionQueryBuffer;

	/*
	* Returns true if per-instance occlusion queries can be rendered for the view.
	*/
	bool IsCompatibleWithView(FViewInfo& View);

private:

	/**
	* Allocates a slot in the output buffer for this view. Views are automatically registered during Render().
	* Returns bit mask with a single set bit that should be used to access the occlusion results.
	* Returns 0 if maximum number of supported views is reached, falling back to no-occlusion-query code path.
	*/
	uint32 RegisterView(FViewInfo& View);

	FRDGBufferRef CurrentInstanceOcclusionQueryBuffer = {};

	static constexpr uint32 MaxViews = 1; // 32; -- YURIY_TODO: we could theoretically support up to 32 views
	TArray<uint32> CurrentRenderedViewIDs;

	uint32 AllocatedNumInstances = 0;
};
