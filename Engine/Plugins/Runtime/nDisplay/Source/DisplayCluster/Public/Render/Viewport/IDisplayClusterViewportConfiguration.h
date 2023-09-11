// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameEnums.h"

class UWorld;
class UDisplayClusterConfigurationData;
class ADisplayClusterRootActor;
class IDisplayClusterViewportManager;
struct FDisplayClusterConfigurationICVFX_StageSettings;
struct FDisplayClusterConfigurationRenderFrame;

/**
* Type of DCRA used by configuration
*/
enum class EDisplayClusterRootActorType : uint8
{
	// This DCRA will be used to render previews. The meshes and preview materials are created at runtime.
	Preview = 0,

	// A reference to DCRA in the scene, used as a source for math calculations and references.
	// Locations in the scene and math data are taken from this DCRA.
	Scene,

	// Reference to DCRA, used as a source of configuration data from DCRA and its components.
	Configuration,

	// This value can only be used in very specific cases:
	// For function GetRootActor() : Return any of the DCRAs that are not nullptr, in ascending order of type: Preview, Scene, Configuration.
	// For function SetRootActor() : Sets all references to DRCA to the specified value.
	Any
};

/**
 * Viewport manager configuration.
 */
class DISPLAYCLUSTER_API IDisplayClusterViewportConfiguration
{
public:
	virtual ~IDisplayClusterViewportConfiguration() = default;

public:
	/**
	 *  Sets a reference to the current world to be rendered in DCRA
	 * @param InWorld - ptr to the world to be rendered
	 */
	virtual void SetCurrentWorld(UWorld* InWorld) = 0;

	/**
	 *  Sets a reference to the DCRA's
	 * @param InRootActor     - a ref to DCRA
	 * @param InRootActorType - Type of the DCRA
	 */
	virtual void SetRootActor(ADisplayClusterRootActor* InRootActor, const EDisplayClusterRootActorType InRootActorType) = 0;

	/**
	* Update\Create\Delete local node viewports
	* Updating the configuration to render a ClusterNode in the specified mode
	*
	* @param InRenderMode     - Render mode
	* @param InClusterNodeId  - cluster node for rendering
	*/
	virtual bool UpdateConfigurationForClusterNode(EDisplayClusterRenderFrameMode InRenderMode, const FString& InClusterNodeId) = 0;

	/**
	* Update\Create\Delete local node viewports
	* Updating the configuration to render a list of viewports in a given mode
	*
	* @param InRenderMode    - Render mode
	* @param InViewportNames - Viewports names for next frame
	*/
	virtual bool UpdateConfigurationForViewportsList(EDisplayClusterRenderFrameMode InRenderMode, const TArray<FString>& InViewportNames) = 0;

public:
	/** Return the configuration proxy object. */
	virtual const class IDisplayClusterViewportConfigurationProxy& GetProxy() const = 0;

	/** Return the viewport manager that used by this configuration. */
	virtual IDisplayClusterViewportManager* GetViewportManager() const = 0;

	/**
	 *  Gets a reference to the current world being rendered in DCRA
	 */
	virtual UWorld* GetCurrentWorld() const = 0;

	/**
	 * Gets a reference to the DCRA by type.
	 * If a DCRA with the specified type is not assigned, the default DCRA is used.
	 * 
	 * @param InRootActorType - Type of the DCRA
	 */
	virtual ADisplayClusterRootActor* GetRootActor(const EDisplayClusterRootActorType InRootActorType) const = 0;

	/**
	 * Gets a configuration data from the Configuration RootActor
	 */
	virtual const UDisplayClusterConfigurationData* GetConfigurationData() const = 0;

	/**
	 * Gets a configuration stage settings from the Configuration RootActor
	 */
	virtual const FDisplayClusterConfigurationICVFX_StageSettings* GetStageSettings() const = 0;

	/**
	 * Gets a configuration render settings from the Configuration RootActor
	 */
	virtual const FDisplayClusterConfigurationRenderFrame* GetConfigurationRenderFrameSettings() const = 0;

	/**
	* Returns true if the current world type is equal to one of the input types.
	*/
	virtual bool IsCurrentWorldHasAnyType(
		const EWorldType::Type InWorldType1,
		const EWorldType::Type InWorldType2 = EWorldType::None,
		const EWorldType::Type InWorldType3 = EWorldType::None
	) const = 0;

	/**
	* Returns true if the given DCRA has a world type equal to one of the input types.
	*/
	virtual bool IsRootActorWorldHasAnyType(
		const EDisplayClusterRootActorType InRootActorType,
		const EWorldType::Type InWorldType1,
		const EWorldType::Type InWorldType2 = EWorldType::None,
		const EWorldType::Type InWorldType3 = EWorldType::None
	) const = 0;

	/** 
	* Returns true if the scene is open now (The current world is assigned and DCRA has already initialized for it).
	*/
	virtual bool IsSceneOpened() const = 0;

	/** Returns true if preview rendering mode is used. */
	virtual bool IsPreviewRendering() const = 0;

	/** Returns the rendering mode for PIE. */
	virtual EDisplayClusterRenderFrameMode GetRenderModeForPIE() const = 0;

	/** Return current cluster node id. */
	virtual const FString& GetClusterNodeId() const = 0;
};
