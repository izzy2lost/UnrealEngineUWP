// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Render/Viewport/IDisplayClusterViewportConfiguration.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationProxy.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "SceneTypes.h"
#include "Misc/DisplayClusterObjectRef.h"
#include "Templates/SharedPointer.h"

class FDisplayClusterViewportConfigurationProxy;
class FDisplayClusterViewportManager;
class FDisplayClusterViewportManagerProxy;
class ADisplayClusterRootActor;
class UDisplayClusterConfigurationData;
struct FDisplayClusterConfigurationRenderFrame;

/**
* Implementation of the viewport manager configuration.
*/
class FDisplayClusterViewportConfiguration
	: public IDisplayClusterViewportConfiguration
	, public TSharedFromThis<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportConfiguration();
	virtual ~FDisplayClusterViewportConfiguration();

	void Initialize(FDisplayClusterViewportManager& InViewportManager);

public:
	//~ BEGIN IDisplayClusterViewportConfiguration
	virtual void SetCurrentWorld(UWorld* InWorld) override;
	virtual void SetRootActor(ADisplayClusterRootActor* InRootActor, const EDisplayClusterRootActorType InRootActorType) override;
	virtual bool UpdateConfigurationForClusterNode(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId) override;
	virtual bool UpdateConfigurationForViewportsList(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames) override;

	virtual UWorld* GetCurrentWorld() const override;
	virtual ADisplayClusterRootActor* GetRootActor(const EDisplayClusterRootActorType InRootActorType) const override;
	virtual IDisplayClusterViewportManager* GetViewportManager() const override;
	virtual const UDisplayClusterConfigurationData* GetConfigurationData() const override;
	virtual const FDisplayClusterConfigurationICVFX_StageSettings* GetStageSettings() const override;
	virtual const FDisplayClusterConfigurationRenderFrame* GetConfigurationRenderFrameSettings() const  override;

	virtual EDisplayClusterRenderFrameMode GetRenderModeForPIE() const override;
	virtual bool IsCurrentWorldHasAnyType(const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2 = EWorldType::None, const EWorldType::Type InWorldType3 = EWorldType::None) const override;
	virtual bool IsRootActorWorldHasAnyType(const EDisplayClusterRootActorType InRootActorType, const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2 = EWorldType::None, const EWorldType::Type InWorldType3 = EWorldType::None) const override;

	virtual const IDisplayClusterViewportConfigurationProxy& GetProxy() const override
	{
		return *Proxy;
	}

	virtual bool IsSceneOpened() const override
	{
		return bCurrentSceneActive && GetCurrentWorld();
	}

	virtual const FString& GetClusterNodeId() const override
	{
		return RenderFrameSettings.ClusterNodeId;
	}

	virtual bool IsPreviewRendering() const override
	{
		return RenderFrameSettings.IsPreviewRendering();
	}
	// ~~END IDisplayClusterViewportConfiguration

public:
	/** Get a pointer to the DC ViewportManager if it still exists. */
	FDisplayClusterViewportManager* GetViewportManagerImpl() const
	{
		return ViewportManagerWeakPtr.IsValid() ? ViewportManagerWeakPtr.Pin().Get() : nullptr;
	}

	/** Gets the rendering frame settings. */
	const FDisplayClusterRenderFrameSettings& GetRenderFrameSettings() const
	{ 
		check(IsInGameThread());

		return RenderFrameSettings;
	}

	/** Sets the rendering frame settings. */
	void SetRenderFrameSettings(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings)
	{
		check(IsInGameThread());

		// Process cluster node name changes
		SetClusterNodeId(InRenderFrameSettings.ClusterNodeId);

		RenderFrameSettings = InRenderFrameSettings;
	}

	/** Set the name of the current cluster node. */
	void SetClusterNodeId(const FString& InClusterNodeId)
	{
		if (RenderFrameSettings.ClusterNodeId != InClusterNodeId)
		{
			bCurrentRenderFrameViewportsNeedsToBeUpdated = true;
			RenderFrameSettings.ClusterNodeId = InClusterNodeId;
		}
	}

private:
	/** Update configuration implementation.
	* 
	* @param InRenderMode - rendering mode.
	* @param InClusterNodeId - (opt) Configuring rendering for a cluster node.
	* @param InViewportNames - (opt) Configuring rendering for a list of viewports.
	*/
	bool ImplUpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId, const TArray<FString>* InViewportNames);

	/** Hide DCRA components for nDisplay rendering.*/
	void ImplUpdateConfigurationVisibility() const;

public:
	// Reference to configuration proxy object
	const TSharedRef<FDisplayClusterViewportConfigurationProxy, ESPMode::ThreadSafe> Proxy;

	// Is current scene started
	bool bCurrentSceneActive = false;

	// Is scene update is required
	bool bCurrentSceneNeedsToBeUpdated = false;

	// Whether the list of viewports of the current frame needs to be updated
	bool bCurrentRenderFrameViewportsNeedsToBeUpdated = false;

private:
	// Current render frame settings
	FDisplayClusterRenderFrameSettings RenderFrameSettings;

	// A reference to the owning viewport manager
	TWeakPtr<FDisplayClusterViewportManager, ESPMode::ThreadSafe> ViewportManagerWeakPtr;

private:
	// This DCRA will be used to render previews. The meshes and preview materials are created at runtime.
	FDisplayClusterActorRef PreviewRootActorRef;

	// (Optional) A reference to DCRA in the scene, used as a source for math calculations and references.
	// Locations in the scene and math data are taken from this DCRA.
	FDisplayClusterActorRef SceneRootActorRef;

	// (Optional) Reference to DCRA, used as a source of configuration data from DCRA and its components.
	FDisplayClusterActorRef ConfigurationRootActorRef;

	// Pointer to the current world to be rendered in.
	TWeakObjectPtr<UWorld> CurrentWorldRef;
};
