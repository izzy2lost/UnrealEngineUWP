// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterRootActor.h"
#include "Engine/World.h"

/**
 * Creates and handle DCRA proxies.
 */
class FDisplayClusterScenePreviewProxyManager
{
public:
	~FDisplayClusterScenePreviewProxyManager();

	/** Assign SceneRootActor to the renderer with the specified ID to create a root proxy actor for it.
	* @param RendererId - ID of renderer that uses this DCRA
	* @param RootActor  - the new scene root actor that used by this renderer
	* 
	* Note: If RootActor is null, it means that the renderer is no longer using the proxy.
	*/
	void SetSceneRootActorForRenderer(int32 RendererId, ADisplayClusterRootActor* RootActor);

	/** Get the DCRA proxy for the renderer by renderer id.
	* 
	* @param RendererId - ID of renderer that uses this DCRA
	* 
	* @return - A reference to the actor's root proxy, if it exists, or nullptr.
	* 
	* Note: You must call SetSceneRootActorForRenderer() for the renderer and the root actor being used before using this function.
	*/
	ADisplayClusterRootActor* GetProxyRootActor(int32 RendererId) const;

	/** This function calls Tick() for a preview world with proxy root actors, which triggers rendering of previews for them.
	* 
	* @param DeltaTime - has passed to PreviewWorld->Tick().
	*/
	void Tick(float DeltaTime);

private:
	void CreatePreviewWorld();
	void DestroyPreviewWorld();

	/** Create RootActor proxy. */
	ADisplayClusterRootActor* CreateRootActorProxy(int32 RendererId, ADisplayClusterRootActor* SceneRootActor);

	void DestroyRootActorProxy(ADisplayClusterRootActor* ProxyRootActor);

private:
	struct FRendererProxy
	{
		// RootActor proxy objects
		TWeakObjectPtr<ADisplayClusterRootActor> ProxyRootActor;

		// RootActor in scene that used to create DCRA proxy
		TWeakObjectPtr<ADisplayClusterRootActor> SceneRootActor;
	};

	/** Registered proxies for renderers by ID. */
	TMap<int32, FRendererProxy> RendererProxies;

	/** The preview world is only used when using the DCRA proxy. */
	TObjectPtr<UWorld> PreviewWorld;
};
