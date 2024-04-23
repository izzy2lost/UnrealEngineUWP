// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterScenePreviewProxyManager.h"

#include "Components/DisplayClusterStageGeometryComponent.h"

FDisplayClusterScenePreviewProxyManager::~FDisplayClusterScenePreviewProxyManager()
{
	DestroyPreviewWorld();
}

void FDisplayClusterScenePreviewProxyManager::CreatePreviewWorld()
{
#if WITH_EDITOR
	if (!PreviewWorld)
	{
		PreviewWorld = UWorld::CreateWorld(EWorldType::None, false);

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::None);
		WorldContext.SetCurrentWorld(PreviewWorld);
	}
#endif
}

void FDisplayClusterScenePreviewProxyManager::DestroyPreviewWorld()
{
#if WITH_EDITOR
	if (PreviewWorld)
	{
		GEngine->DestroyWorldContext(PreviewWorld);
		PreviewWorld->DestroyWorld(false);
	}
#endif

	PreviewWorld = nullptr;
}

void FDisplayClusterScenePreviewProxyManager::Tick(float DeltaTime)
{
	if (PreviewWorld)
	{
		PreviewWorld->Tick(ELevelTick::LEVELTICK_All, DeltaTime);
	}
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewProxyManager::CreateRootActorProxy(int32 RendererId, ADisplayClusterRootActor* SceneRootActor)
{
	if (!SceneRootActor)
	{
		return nullptr;
	}

	ADisplayClusterRootActor* RootActorProxy = nullptr;

#if WITH_EDITOR
	// Create new preview world for DCRA proxy
	CreatePreviewWorld();
	if (!PreviewWorld)
	{
		return nullptr;
	}

	// Create a proxy for render
	FObjectDuplicationParameters DupeActorParameters(SceneRootActor, PreviewWorld->GetCurrentLevel());
	DupeActorParameters.FlagMask = RF_AllFlags & ~(RF_ArchetypeObject | RF_Transactional); // Keeps archetypes correct in config data.
	DupeActorParameters.PortFlags = PPF_DuplicateVerbatim;
	DupeActorParameters.DestName = FName(FString::Printf(TEXT("Preview%i_%s"), RendererId, *SceneRootActor->GetName()));

	RootActorProxy = CastChecked<ADisplayClusterRootActor>(StaticDuplicateObjectEx(DupeActorParameters));

	// Use root actor from scene to render
	if (IDisplayClusterViewportManager* ViewportManager = RootActorProxy ? RootActorProxy->GetOrCreateViewportManager() : nullptr)
	{
		// Using DCRA from the scene for rendering
		ViewportManager->GetConfiguration().SetRootActor(EDisplayClusterRootActorType::Scene | EDisplayClusterRootActorType::Configuration, SceneRootActor);
	}

	RootActorProxy->SetFlags(RF_Transient); // This signals to the stage actor it is a proxy

	PreviewWorld->GetCurrentLevel()->AddLoadedActor(RootActorProxy);

	// Draw the geometry map for the proxy stage actor immediately to avoid a race condition where the geometry map could render
	// before the actor location changes propagate to its component proxies, resulting in an inaccurate proxy geometry map
	RootActorProxy->GetStageGeometryComponent()->Invalidate(true);

	// Spawned actor will take the transform values from the template, so manually reset them to zero here
	RootActorProxy->SetActorLocation(FVector::ZeroVector);
	RootActorProxy->SetActorRotation(FRotator::ZeroRotator);

	// Set translucency sort priority of root actor proxy primitive components so that actors that are flush with screens are rendered on top of them
	RootActorProxy->ForEachComponent<UPrimitiveComponent>(false, [](UPrimitiveComponent* InPrimitiveComponent)
	{
		InPrimitiveComponent->SetTranslucentSortPriority(-10);
	});
#endif

	return RootActorProxy;
}

void FDisplayClusterScenePreviewProxyManager::DestroyRootActorProxy(ADisplayClusterRootActor* ProxyRootActor)
{
#if WITH_EDITOR
	if (PreviewWorld && ProxyRootActor)
	{
		PreviewWorld->GetCurrentLevel()->RemoveLoadedActors({ ProxyRootActor });
	}
#endif
}

void FDisplayClusterScenePreviewProxyManager::SetSceneRootActorForRenderer(int32 RendererId, ADisplayClusterRootActor* SceneRootActor)
{
	if (RendererProxies.Contains(RendererId))
	{
		FRendererProxy& RendererProxy = RendererProxies[RendererId];

		if (RendererProxy.SceneRootActor == SceneRootActor)
		{
			// Re-use existing proxy
			return;
		}

		// The root agent has changed, destroy the proxy currently in use.
		DestroyRootActorProxy(RendererProxy.ProxyRootActor.Get());

		RendererProxies.Remove(RendererId);
	}

	if (ADisplayClusterRootActor* ProxyRootActor = CreateRootActorProxy(RendererId, SceneRootActor))
	{
		// Create new proxy
		FRendererProxy RendererProxy;
		RendererProxy.SceneRootActor = SceneRootActor;
		RendererProxy.ProxyRootActor = ProxyRootActor;

		RendererProxies.Emplace(RendererId, RendererProxy);
	}


	// Destroy preview world if not used
	if (RendererProxies.IsEmpty())
	{
		DestroyPreviewWorld();
	}
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewProxyManager::GetProxyRootActor(int32 RendererId) const
{
	if (RendererProxies.Contains(RendererId))
	{
		return RendererProxies[RendererId].ProxyRootActor.Get();
	}

	return nullptr;
}
