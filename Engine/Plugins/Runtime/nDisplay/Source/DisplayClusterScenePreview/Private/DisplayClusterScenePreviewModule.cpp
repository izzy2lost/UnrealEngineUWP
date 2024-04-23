// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterScenePreviewModule.h"

#include "Blueprints/DisplayClusterBlueprintLib.h"
#include "CanvasTypes.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "DisplayClusterChromakeyCardActor.h"
#include "DisplayClusterLightCardActor.h"
#include "DisplayClusterRootActor.h"
#include "Engine/Blueprint.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/TransactionObjectEvent.h"
#include "TextureResource.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

#define LOCTEXT_NAMESPACE "DisplayClusterScenePreview"

static TAutoConsoleVariable<float> CVarDisplayClusterScenePreviewRenderTickDelay(
	TEXT("nDisplay.ScenePreview.RenderTickDelay"),
	0.1f,
	TEXT("The number of seconds to wait between processing queued renders.")
);

void FDisplayClusterScenePreviewModule::StartupModule()
{
}

void FDisplayClusterScenePreviewModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RenderTickerHandle);
	RenderTickerHandle.Reset();

	TArray<int32> RendererIds;
	RendererConfigs.GenerateKeyArray(RendererIds);
	for (const int32 RendererId : RendererIds)
	{
		DestroyRenderer(RendererId);
	}
}

int32 FDisplayClusterScenePreviewModule::CreateRenderer()
{
	FRendererConfig& Config = RendererConfigs.Add(NextRendererId);

	Config.Renderer = MakeShared<FDisplayClusterMeshProjectionRenderer>();

	return NextRendererId++;
}

bool FDisplayClusterScenePreviewModule::DestroyRenderer(int32 RendererId)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		// Release the proxy resources that were used by this renderer.
		ProxyManager.SetSceneRootActorForRenderer(RendererId, nullptr);

		RegisterRootActorEvents(RendererId, *Config, false);
		RendererConfigs.Remove(RendererId);

		RegisterOrUnregisterGlobalActorEvents();

		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererRootActorPath(int32 RendererId, const FString& ActorPath, const FDisplayClusterRootActorPropertyOverrides& InPropertyOverrides, const EDisplayClusterScenePreviewFlags PreviewFlags)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->RootActorPath = ActorPath;

		ADisplayClusterRootActor* RootActor = FindObject<ADisplayClusterRootActor>(nullptr, *ActorPath);
		InternalSetRendererRootActor(RendererId, *Config, RootActor, PreviewFlags);

		// Use custom properties on root actor
		Config->RootActorPropertyOverrides = InPropertyOverrides;
		InternalOverridePropertiesForRendererRootActor(RendererId, *Config);

		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererRootActor(int32 RendererId, ADisplayClusterRootActor* Actor, const FDisplayClusterRootActorPropertyOverrides& InPropertyOverrides, const EDisplayClusterScenePreviewFlags PreviewFlags)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->RootActorPath.Empty();
		Config->RootActorPropertyOverrides = InPropertyOverrides;

		InternalSetRendererRootActor(RendererId, *Config, Actor, PreviewFlags);

		// Use custom properties on root actor
		Config->RootActorPropertyOverrides = InPropertyOverrides;
		InternalOverridePropertiesForRendererRootActor(RendererId, *Config);

		return true;
	}

	return false;
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewModule::GetRendererRootActor(int32 RendererId)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		return InternalGetRendererRootActorOrProxy(RendererId, *Config);
	}

	return nullptr;
}

bool FDisplayClusterScenePreviewModule::GetActorsInRendererScene(int32 RendererId, bool bIncludeRoot, TArray<AActor*>& OutActors)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		if (bIncludeRoot)
		{
			OutActors.Add(GetRendererRootActor(RendererId));
		}

		for (const TWeakObjectPtr<AActor>& Actor : Config->AddedActors)
		{
			if (!Actor.IsValid())
			{
				continue;
			}

			OutActors.Add(Actor.Get());
		}

		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::AddActorToRenderer(int32 RendererId, AActor* Actor)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->AddActor(Actor);
		Config->AddedActors.Add(Actor);
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::AddActorToRenderer(int32 RendererId, AActor* Actor, const TFunctionRef<bool(const UPrimitiveComponent*)>& PrimitiveFilter)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->AddActor(Actor, PrimitiveFilter);
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::RemoveActorFromRenderer(int32 RendererId, AActor* Actor)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->RemoveActor(Actor);
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::ClearRendererScene(int32 RendererId)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->ClearScene();
		Config->AddedActors.Empty();
		Config->AutoActors.Empty();
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererActorSelectedDelegate(int32 RendererId, FDisplayClusterMeshProjectionRenderer::FSelection ActorSelectedDelegate)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->ActorSelectedDelegate = ActorSelectedDelegate;
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererRenderSimpleElementsDelegate(int32 RendererId, FDisplayClusterMeshProjectionRenderer::FSimpleElementPass RenderSimpleElementsDelegate)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->RenderSimpleElementsDelegate = RenderSimpleElementsDelegate;
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::Render(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, FCanvas& Canvas)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		return InternalRenderImmediate(RendererId, *Config, RenderSettings, Canvas);
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::RenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, const FIntPoint& Size, FRenderResultDelegate ResultDelegate)
{
	return InternalRenderQueued(RendererId, RenderSettings, nullptr, Size, ResultDelegate);
}

bool FDisplayClusterScenePreviewModule::RenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, const TWeakPtr<FCanvas> Canvas, FRenderResultDelegate ResultDelegate)
{
	if (!Canvas.IsValid())
	{
		return false;
	}

	FRenderTarget* RenderTarget = Canvas.Pin()->GetRenderTarget();
	if (!RenderTarget)
	{
		return false;
	}

	return InternalRenderQueued(RendererId, RenderSettings, Canvas, RenderTarget->GetSizeXY(), ResultDelegate);
}

bool FDisplayClusterScenePreviewModule::IsRealTimePreviewEnabled() const
{
	return bIsRealTimePreviewEnabled;
}

bool FDisplayClusterScenePreviewModule::IsBlueprintMatchesRendererRootActor(int32 RendererId, FRendererConfig& RendererConfig, UBlueprint* Blueprint)
{
#if WITH_EDITOR
	if (RendererConfig.RootActor.IsValid() && Blueprint == UBlueprint::GetBlueprintFromClass(RendererConfig.RootActor->GetClass()))
	{
		return true;
	}
#endif

	return false;
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewModule::InternalGetRendererRootActor(int32 RendererId, FRendererConfig& RendererConfig)
{
	if (!RendererConfig.RootActor.IsValid() && !RendererConfig.RootActorPath.IsEmpty())
	{
		// Try to find the actor by its path
		ADisplayClusterRootActor* RootActor = FindObject<ADisplayClusterRootActor>(nullptr, *RendererConfig.RootActorPath);
		if (RootActor)
		{
			InternalSetRendererRootActor(RendererId, RendererConfig, RootActor, RendererConfig.PreviewFlags);
			InternalOverridePropertiesForRendererRootActor(RendererId, RendererConfig);
		}
	}

	return RendererConfig.RootActor.Get();
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewModule::InternalGetRendererRootActorOrProxy(int32 RendererId, FRendererConfig& RendererConfig)
{
	// Note: we call this function first because it can assign a new root actor and proxy.
	ADisplayClusterRootActor* RootActor = InternalGetRendererRootActor(RendererId, RendererConfig);

	// When proxy is required, get it from ProxyManager.
	const bool bUseRootActorProxy = EnumHasAnyFlags(RendererConfig.PreviewFlags, EDisplayClusterScenePreviewFlags::UseRootActorProxy);

	return bUseRootActorProxy ? ProxyManager.GetProxyRootActor(RendererId) : RootActor;
}

bool FDisplayClusterScenePreviewModule::InternalRenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, TWeakPtr<FCanvas> Canvas,
	const FIntPoint& Size, FRenderResultDelegate ResultDelegate)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		RenderQueue.Enqueue(FPreviewRenderJob(RendererId, RenderSettings, Size, Canvas, ResultDelegate));

		if (!RenderTickerHandle.IsValid())
		{
			RenderTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateRaw(this, &FDisplayClusterScenePreviewModule::OnTick),
				CVarDisplayClusterScenePreviewRenderTickDelay.GetValueOnGameThread()
			);
		}

		return true;
	}

	return false;
}


void FDisplayClusterScenePreviewModule::InternalOverridePropertiesForRendererRootActor(int32 RendererId, FRendererConfig& RendererConfig)
{
	// Override root actor properties:
	if (ADisplayClusterRootActor* RendererRootActor = InternalGetRendererRootActorOrProxy(RendererId, RendererConfig))
	{
		RendererRootActor->OverrideRootActorProperties(RendererConfig.RootActorPropertyOverrides);
	}
}

void FDisplayClusterScenePreviewModule::InternalSetRendererRootActor(int32 RendererId, FRendererConfig& RendererConfig, ADisplayClusterRootActor* Actor, const EDisplayClusterScenePreviewFlags PreviewFlags)
{
	// Determine these values before we update the config's RootActor/bAutoUpdateLightcards
	const bool bRootChanged = RendererConfig.RootActor != Actor;

	if (bRootChanged || RendererConfig.PreviewFlags != PreviewFlags)
	{
		// Unregister events for the previous cluster
		const bool bAutoUpdateLightcards = EnumHasAnyFlags(RendererConfig.PreviewFlags, EDisplayClusterScenePreviewFlags::AutoUpdateLightcards);
		if (bAutoUpdateLightcards)
		{
			RegisterRootActorEvents(RendererId, RendererConfig, false);
		}

		RendererConfig.RootActor = Actor;
		RendererConfig.PreviewFlags = PreviewFlags;

		// Updates the proxy for the new root actor.
		const bool bUseRootActorProxy = EnumHasAnyFlags(RendererConfig.PreviewFlags, EDisplayClusterScenePreviewFlags::UseRootActorProxy);
		ProxyManager.SetSceneRootActorForRenderer(RendererId, bUseRootActorProxy ? Actor : nullptr);

		AutoPopulateScene(RendererId, RendererConfig);
	}

	RegisterRootActorEvents(RendererId, RendererConfig, true);
	RegisterOrUnregisterGlobalActorEvents();
}

bool FDisplayClusterScenePreviewModule::InternalRenderImmediate(int32 RendererId, FRendererConfig& RendererConfig, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, FCanvas& Canvas)
{
	// Update this so that whoever gets the callback can immediately check whether the nDisplay preview may be out of date
	UpdateIsRealTimePreviewEnabled();

	// Get the Root Actor or proxy for rendering previews.
	ADisplayClusterRootActor* RootActor = InternalGetRendererRootActorOrProxy(RendererId, RendererConfig);
	UWorld* World = RootActor ? RootActor->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	const bool bAutoUpdateLightcards = EnumHasAnyFlags(RendererConfig.PreviewFlags, EDisplayClusterScenePreviewFlags::AutoUpdateLightcards);
	if (bAutoUpdateLightcards && RendererConfig.bIsSceneDirty)
	{
		AutoPopulateScene(RendererId, RendererConfig);
	}

	// Push any deferred render state updates to ensure that light card positions, preview meshes modified above, etc. are up to date
	World->SendAllEndOfFrameUpdates();

	RendererConfig.Renderer->Render(&Canvas, World->Scene, RenderSettings);

	return true;
}

void FDisplayClusterScenePreviewModule::RegisterOrUnregisterGlobalActorEvents()
{
	// Check whether any of our configs need actor events
	bool bShouldBeRegistered = false;
	for (const TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		const bool bAutoUpdateLightcards = EnumHasAnyFlags(ConfigPair.Value.PreviewFlags, EDisplayClusterScenePreviewFlags::AutoUpdateLightcards);
		if (bAutoUpdateLightcards)
		{
			bShouldBeRegistered = true;
			break;
		}
	}

#if WITH_EDITOR
	if (bShouldBeRegistered && !bIsRegisteredForActorEvents)
	{
		// Register for events
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FDisplayClusterScenePreviewModule::OnActorPropertyChanged);
		FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FDisplayClusterScenePreviewModule::OnObjectTransacted);

		if (GEngine != nullptr)
		{
			GEngine->OnLevelActorDeleted().AddRaw(this, &FDisplayClusterScenePreviewModule::OnLevelActorDeleted);
			GEngine->OnLevelActorAdded().AddRaw(this, &FDisplayClusterScenePreviewModule::OnLevelActorAdded);
		}
	}
	else if (!bShouldBeRegistered && bIsRegisteredForActorEvents)
	{
		// Unregister for events
		FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
		FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

		if (GEngine != nullptr)
		{
			GEngine->OnLevelActorDeleted().RemoveAll(this);
			GEngine->OnLevelActorAdded().RemoveAll(this);
		}
	}
#endif
}

void FDisplayClusterScenePreviewModule::RegisterRootActorEvents(int32 RendererId, FRendererConfig& RendererConfig, bool bShouldRegister)
{
#if WITH_EDITOR
	ADisplayClusterRootActor* Actor = RendererConfig.RootActor.Get();
	if (!Actor)
	{
		return;
	}

	const uint8* GenericThis = reinterpret_cast<uint8*>(this);

	// Register/unregister for Blueprint events
	if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Actor->GetClass()))
	{
		Blueprint->OnCompiled().RemoveAll(this);

		if (bShouldRegister)
		{
			Blueprint->OnCompiled().AddRaw(this, &FDisplayClusterScenePreviewModule::OnBlueprintCompiled);
		}
	}
#endif
}

void FDisplayClusterScenePreviewModule::AutoPopulateScene(int32 RendererId, FRendererConfig& RendererConfig)
{
	const bool bAutoUpdateLightcards = EnumHasAnyFlags(RendererConfig.PreviewFlags, EDisplayClusterScenePreviewFlags::AutoUpdateLightcards);
	if (bAutoUpdateLightcards)
	{
		RendererConfig.Renderer->ClearScene();
		RendererConfig.AddedActors.Empty();
		RendererConfig.AutoActors.Empty();
	}
	
	// The renderer can use a proxy.
	if (ADisplayClusterRootActor* RootActor = InternalGetRendererRootActorOrProxy(RendererId, RendererConfig))
	{
		TArray<FString> ProjectionMeshNames;

		if (UDisplayClusterConfigurationData* Config = RootActor->GetConfigData())
		{
			Config->GetReferencedMeshNames(ProjectionMeshNames);
		}

		RendererConfig.Renderer->AddActor(RootActor, [&ProjectionMeshNames](const UPrimitiveComponent* PrimitiveComponent)
		{
			// Filter out any primitive component that isn't a projection mesh (a static mesh that has a Mesh projection configured for it) or a screen component
			const bool bIsProjectionMesh = PrimitiveComponent->IsA<UStaticMeshComponent>() && ProjectionMeshNames.Contains(PrimitiveComponent->GetName());
			const bool bIsScreen = PrimitiveComponent->IsA<UDisplayClusterScreenComponent>();
			return bIsProjectionMesh || bIsScreen;
		});

		if (bAutoUpdateLightcards)
		{
			// Automatically add the lightcards found on this actor
			TSet<ADisplayClusterLightCardActor*> LightCards;
			UDisplayClusterBlueprintLib::FindLightCardsForRootActor(RootActor, LightCards);
			
			TSet<ADisplayClusterChromakeyCardActor*> ChromaKeyCards;
			UDisplayClusterBlueprintLib::FindChromakeyCardsForRootActor(RootActor, ChromaKeyCards);
			LightCards.Append(reinterpret_cast<TSet<ADisplayClusterLightCardActor*>&>(ChromaKeyCards));
			
			TSet<AActor*> Actors;
			Actors.Reserve(LightCards.Num());
			for (ADisplayClusterLightCardActor* LightCard : LightCards)
			{
				Actors.Add(LightCard);
			}

			// Also check for any non-lightcard actors in the world that are valid to control from ICVFX editors
			if (UWorld* World = RootActor->GetWorld())
			{
				for (const TWeakObjectPtr<AActor> WeakActor : TActorRange<AActor>(World))
				{
					if (WeakActor.IsValid() && WeakActor->Implements<UDisplayClusterStageActor>() && !WeakActor->IsA<ADisplayClusterLightCardActor>())
					{
						Actors.Add(WeakActor.Get());
					}
				}
			}

			for (AActor* Actor : Actors)
			{
				if (RendererConfig.AddedActors.Contains(Actor))
				{
					continue;
				}

				RendererConfig.Renderer->AddActor(Actor);
				RendererConfig.AddedActors.Add(Actor);
				RendererConfig.AutoActors.Add(Actor);
			}
		}
	}

	RendererConfig.bIsSceneDirty = false;
}

bool FDisplayClusterScenePreviewModule::UpdateIsRealTimePreviewEnabled()
{
#if WITH_EDITOR
	bIsRealTimePreviewEnabled = false;

	if (!GEditor)
	{
		return false;
	}

	for (const FLevelEditorViewportClient* LevelViewport : GEditor->GetLevelViewportClients())
	{
		if (LevelViewport && LevelViewport->IsRealtime())
		{
			bIsRealTimePreviewEnabled = true;
			break;
		}
	}

	return bIsRealTimePreviewEnabled;
#else
	return false;
#endif
}

bool FDisplayClusterScenePreviewModule::OnTick(float DeltaTime)
{
	// This function calls Tick() for a preview world with proxy root actors, which triggers rendering of previews for them.
	ProxyManager.Tick(DeltaTime);

	// This loop should break when we either run out of jobs or complete a single job
	while (!RenderQueue.IsEmpty())
	{
		FPreviewRenderJob Job;
		if (!RenderQueue.Dequeue(Job))
		{
			break;
		}

		ensure(Job.ResultDelegate.IsBound());

		if (FRendererConfig* Config = RendererConfigs.Find(Job.RendererId))
		{
			if (Job.bWasCanvasProvided)
			{
				// We were provided a canvas for this render job, so use it if possible
				if (!Job.Canvas.IsValid())
				{
					Job.ResultDelegate.Execute(nullptr);
					continue;
				}

				TSharedPtr<FCanvas, ESPMode::ThreadSafe> Canvas = Job.Canvas.Pin();
				FRenderTarget* RenderTarget = Canvas->GetRenderTarget();
				if (!RenderTarget)
				{
					Job.ResultDelegate.Execute(nullptr);
					continue;
				}

				InternalRenderImmediate(Job.RendererId, *Config, Job.Settings, *Canvas);
				Job.ResultDelegate.Execute(RenderTarget);
				break;
			}

			ADisplayClusterRootActor* RootActor = InternalGetRendererRootActorOrProxy(Job.RendererId, *Config);
			if (UWorld* World = RootActor ? RootActor->GetWorld() : nullptr)
			{
				// We need to provide the render target for this job
				UTextureRenderTarget2D* RenderTarget = Config->RenderTarget.Get();

				if (!RenderTarget)
				{
					// Create a new render target (which will be reused for this config in the future)
					RenderTarget = NewObject<UTextureRenderTarget2D>();
					RenderTarget->InitCustomFormat(Job.Size.X, Job.Size.Y, PF_B8G8R8A8, true);

					Config->RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(RenderTarget);
				}
				else if (RenderTarget->SizeX != Job.Size.X || RenderTarget->SizeY != Job.Size.Y)
				{
					// Resize to match the new size
					RenderTarget->ResizeTarget(Job.Size.X, Job.Size.Y);

					// Flush commands so target is immediately ready to render at the new size
					FlushRenderingCommands();
				}

				FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
				FCanvas Canvas(RenderTargetResource, nullptr, FGameTime::GetTimeSinceAppStart(), World->Scene->GetFeatureLevel());

				InternalRenderImmediate(Job.RendererId, *Config, Job.Settings, Canvas);
				Job.ResultDelegate.Execute(RenderTargetResource);
				break;
			}
			
			// No canvas and no world, so try the next render
		}

		// Config no longer exists, so try the next render
		Job.ResultDelegate.Execute(nullptr);
	}

	if (RenderQueue.IsEmpty())
	{
		RenderTickerHandle.Reset();
		return false;
	}

	return true;
}

void FDisplayClusterScenePreviewModule::OnActorPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		FRendererConfig& Config = ConfigPair.Value;
		const bool bAutoUpdateLightcards = EnumHasAnyFlags(Config.PreviewFlags, EDisplayClusterScenePreviewFlags::AutoUpdateLightcards);
		if (bAutoUpdateLightcards)
		{
			if (Config.RootActor == ObjectBeingModified)
			{
				Config.bIsSceneDirty = true;
				continue;
			}

			if (UActorComponent* Component = Cast<UActorComponent>(ObjectBeingModified))
			{
				if (Component->GetOwner() == Config.RootActor)
				{
					Config.bIsSceneDirty = true;
					continue;
				}
			}
		}
	}
}

void FDisplayClusterScenePreviewModule::OnLevelActorDeleted(AActor* Actor)
{
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		FRendererConfig& Config = ConfigPair.Value;
		if (Config.AutoActors.Contains(Actor))
		{
			Config.bIsSceneDirty = true;
		}
	}
}

void FDisplayClusterScenePreviewModule::OnLevelActorAdded(AActor* Actor)
{
	if (!Actor || !Actor->Implements<UDisplayClusterStageActor>())
	{
		return;
	}

	// The actor won't be added to a root actor yet, so we can't check who it belongs to. Easier to just mark all configs as dirty.
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		ConfigPair.Value.bIsSceneDirty = true;
	}
}

void FDisplayClusterScenePreviewModule::OnBlueprintCompiled(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		int32 RendererId = ConfigPair.Key;
		FRendererConfig& Config = ConfigPair.Value;

		if(IsBlueprintMatchesRendererRootActor(RendererId, Config, Blueprint))
		{
			Config.bIsSceneDirty = true;
			RegisterRootActorEvents(RendererId, Config, true);
		}
	}
#endif
}

void FDisplayClusterScenePreviewModule::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& TransactionObjectEvent)
{
	if (TransactionObjectEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
		{
			ConfigPair.Value.bIsSceneDirty = true;
		}
	}
}

IMPLEMENT_MODULE(FDisplayClusterScenePreviewModule, DisplayClusterScenePreview);

#undef LOCTEXT_NAMESPACE
