// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FCustomRenderPass.h: Custom render pass implementation.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"
#include "Engine/EngineTypes.h"

/** Base class of the custom render pass. Create a derived class to provide custom behaviour. 
	Custom render pass is rendered as part of the main render loop. */
class FCustomRenderPass
{
public:
	FCustomRenderPass()
	{}

	virtual ~FCustomRenderPass() {}

	/** Called before the pass is rendered on render thread, use it for prep tasks such as allocating render target. */
	virtual void PreRender(FRDGBuilder& GraphBuilder) {}
	/** Called after the pass is rendered on render thread, use it for post processing tasks such as applying additional shader effects to the render target. */
	virtual void PostRender(FRDGBuilder& GraphBuilder) {}

	/** Which render passes are needed for the custom render pass. */
	enum ERenderMode
	{
		/** Render depth pre-pass only. */
		ERenderMode_DepthPass,
		/** Render depth pre-pass and base pass. */
		ERenderMode_DepthAndBasePass
	} RenderMode;

	/** The output type into the render target. What type is valid depends on ERenderMode. */
	enum ERenderOutput
	{
		/** Used with ERenderMode_DepthPass. */
		ERenderOutput_SceneDepth,
		ERenderOutput_DeviceDepth,
		/** Used with ERenderMode_DepthAndBasePass. */
		ERenderOutput_SceneColorAndDepth
	} RenderOutput;

	/** Convert output type into corresponding scene capture source type. This is because we use CopySceneCaptureComponentToTarget helper function 
		to copy the custom render pass render result from scene texture into the pass's render target. */
	ESceneCaptureSource GetSceneCaptureSource() const
	{
		if (RenderOutput == ERenderOutput_SceneDepth)
			return SCS_SceneDepth;
		else if (RenderOutput == ERenderOutput_DeviceDepth)
			return SCS_DeviceDepth;
		else if (RenderOutput == ERenderOutput_SceneColorAndDepth)
			return SCS_SceneColorSceneDepth;
		else
			return SCS_MAX;
	}

	FString Name;
	FRDGTextureRef RenderTargetTexture = nullptr;
	FIntPoint RenderTargetSize;

	/** The views created for the custom render pass. */
	TArray<class FViewInfo*> Views;

	/** Optional user data providing a hook into the engine to override settings for this render pass.
		Create a variable with the settings to override in the derived FCustomRenderPass, assign its pointer to UserData.
		Then inside the engine where the setting needs hi-jacking, cast UserData to the target variable type and override the values. */
	void* UserData = nullptr;
};
