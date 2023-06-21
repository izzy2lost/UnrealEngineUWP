// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationInstanceData.h"

class UDisplayClusterConfigurationData;

/**
 * A helper class that creates/updates/deletes viewports for the current rendering frame.
 */
struct FDisplayClusterViewportConfigurationBase
{
public:
	FDisplayClusterViewportConfigurationBase(FDisplayClusterViewportManager& InViewportManager, ADisplayClusterRootActor& InRootActor, const UDisplayClusterConfigurationData& InConfigurationData, FDisplayClusterRenderFrameSettings& InRenderFrameSettings)
		: RootActor(InRootActor)
		, ViewportManager(InViewportManager)
		, RenderFrameSettings(InRenderFrameSettings)
		, ConfigurationData(InConfigurationData)
	{}

public:
	/** Updates the list of viewports for the specified cluster node name, using ConfigurationData.
	 * 
	 * @param ClusterNodeId - cluster node name (special names are supported)
	 */
	void UpdateClusterNodeViewports(const FString& ClusterNodeId);

	/** Updates only the viewports for the specified list with the names of the viewports using ConfigurationData.
	 * 
	 * @param InViewportNames - a list of viewport names to create or update.
	 */
	void UpdateCustomViewports(const TArray<FString>& InViewportNames);

	/** Update postprocess for cluster node.
	 * 
	 * @param ClusterNodeId - cluster node name (special names are supported)
	 */
	void UpdateClusterNodePostProcess(const FString& ClusterNodeId);

protected:
	/**
	 * By default, all postprocess parameters are defined in the FDisplayClusterConfigurationPostprocess structure.
	 * But some post-processes may use their own logic to enable or disable.
	 * 
	 * For example, TextureSharePP will only be enabled with a special condition:
	 * 1. The EnableTextureShare checkbox is checked in the cluster node settings
	 * 2. The TextureShare plugin is enabled for this project.
	 */
	void AddInternalPostprocess(const FString& InPostprocessName);

private:
	/** Update viewports instances in ViewportManager. */
	void ImplUpdateViewports();

	/** Update all viewports warp policies. */
	void ImplUpdateViewportsWarpPolicy();

	/** Initialize variable EntireClusterViewports from configuration. */
	void ImplInitializeEntireClusterViewportsList();

	/** Find the data of the viewport instance by name in the entire cluster. */
	FDisplayClusterViewportConfigurationInstanceData const* ImplFindViewportInEntireCluster(const FString& InViewportId) const;

	/** Find the viewport instance data by name in the current rendering frame. */
	FDisplayClusterViewportConfigurationInstanceData const* ImplFindCurrentFrameViewports(const FString& InViewportId) const;

private:
	ADisplayClusterRootActor& RootActor;
	FDisplayClusterViewportManager& ViewportManager;
	FDisplayClusterRenderFrameSettings& RenderFrameSettings;
	const UDisplayClusterConfigurationData& ConfigurationData;

	/**
	 * The PP is updated from the configuration every frame.
	 * If the post process name is unknown or initialization fails, an error message appears in the log.
	 * To prevent duplicate log error messages in subsequent frames, this PP name is added to the DisabledPostprocessNames list.
	 *
	 * All PPs with names from this list will be ignored.
	 */
	static TArray<FString> DisabledPostprocessNames;

	/**
	 * This post-processing list is updated at runtime. See AddInternalPostprocess()
	 */
	TArray<FString> InternalPostprocessNames;

	// The entire cluster viewports
	TArray<FDisplayClusterViewportConfigurationInstanceData> EntireClusterViewports;

	// the viewports of the current rendering frame (determined from the cluster node name or from the user's list of viewport names)
	TArray<FDisplayClusterViewportConfigurationInstanceData> CurrentFrameViewports;
};
