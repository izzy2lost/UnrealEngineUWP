// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/AvalancheGameInstance.h"

#include "AudioDevice.h"
#include "AvaBlueprint.h"
#include "AvaRemoteControlRebind.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/TimeGuard.h"
#include "Slate/SceneViewport.h"
#include "Viewport/AvaCameraManager.h"

namespace UE::AvalancheGameInstance::Private
{
	/* Frame number marked where a synchronous asset loading occured. */
	static uint64 MarkedSynchronousAssetLoadingFrame = 0;

	/* Frame number marked where delta seconds are ignored. */
	static uint64 MarkedIgnoreDeltaSecondsFrame = 0;

	static void UpdateFrameMarkers()
	{
		// If the last frame had sync asset loading, mark current frame to ignore delta seconds.
		if (MarkedSynchronousAssetLoadingFrame != 0 && GFrameCounter > MarkedSynchronousAssetLoadingFrame)
		{
			MarkedIgnoreDeltaSecondsFrame = GFrameCounter;
			MarkedSynchronousAssetLoadingFrame = 0;
		}

		// Reset marker if stale.
		if (MarkedIgnoreDeltaSecondsFrame != 0 && GFrameCounter > MarkedIgnoreDeltaSecondsFrame)
		{
			MarkedIgnoreDeltaSecondsFrame = 0;
		}
	}
	
	static void MarkSynchronousAssetLoadingThisFrame()
	{
		UpdateFrameMarkers();
		MarkedSynchronousAssetLoadingFrame = GFrameCounter;
	}
	
	static bool ShouldIgnoreDeltaSecondsForCurrentFrame()
	{
		UpdateFrameMarkers();
		return MarkedIgnoreDeltaSecondsFrame != 0 && GFrameCounter == MarkedIgnoreDeltaSecondsFrame;
	}
}

UAvalancheGameInstance::FOnAvaGameInstanceEvent UAvalancheGameInstance::OnEndPlay;
UAvalancheGameInstance::FOnAvaGameInstanceEvent UAvalancheGameInstance::OnRenderTargetReady;

UAvalancheGameInstance* UAvalancheGameInstance::Create(UObject* InOuter, const TSoftObjectPtr<UAvalancheBlueprint>& InAvalancheBlueprintTemplate)
{
	UAvalancheGameInstance* GameInstance = NewObject<UAvalancheGameInstance>(InOuter ? InOuter : GEngine);
	GameInstance->CreateWorld();
	GameInstance->LoadAvalancheBlueprint(InAvalancheBlueprintTemplate);
	return GameInstance;
}

UAvalancheGameInstance* UAvalancheGameInstance::Create(UObject* InOuter)
{
	UAvalancheGameInstance* GameInstance = NewObject<UAvalancheGameInstance>(InOuter ? InOuter : GEngine);

	// We need to call Init() (and corresponding Shutdown() later) to get SubsystemCollection to initialize.
	// Not doing it because it might have side effects.
	//GameInstance->Init();

	GameInstance->CreateWorld();
	return GameInstance;
}

bool UAvalancheGameInstance::CreateWorld()
{
	if (bWorldCreated)
	{
		return false;
	}
	
	TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::CreateWorld);

	if (!EnginePreExitHandle.IsValid())
	{
		EnginePreExitHandle = FCoreDelegates::OnEnginePreExit.AddUObject(this, &UAvalancheGameInstance::OnEnginePreExit);
	}
	
	constexpr EWorldType::Type WorldType = EWorldType::Game;
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::LoadInstance::InitWorld);
		
		const FName AvalancheWorldName = MakeUniqueObjectName(GetOuter(), UWorld::StaticClass(), TEXT("AvalancheGameInstanceWorld"));
		
		PlayWorld = NewObject<UWorld>(this, AvalancheWorldName, RF_Transient);
		check(PlayWorld);
		PlayWorld->WorldType = WorldType;
		PlayWorld->InitializeNewWorld(UWorld::InitializationValues()
			.AllowAudioPlayback(true)
			.CreatePhysicsScene(true)
			.RequiresHitProxies(false)
			.CreateNavigation(true)
			.CreateAISystem(false)	// Disabling AI System until supported in Avalanche.
			.ShouldSimulatePhysics(true)
			.SetTransactional(false)
			//TODO: World Settings
		);

		// Disallow the Engine to tick this World as this Game Instance will be in charge of ticking it
		PlayWorld->SetShouldTick(false);
	}

	WorldContext = &GEngine->CreateNewWorldContext(WorldType);
	check(WorldContext);
	WorldContext->OwningGameInstance = this;
	WorldContext->SetCurrentWorld(PlayWorld.Get());

	PlayWorld->SetGameInstance(this);
	PlayWorld->SetGameMode(FURL());

	bWorldCreated = true;
	return true;
}

bool UAvalancheGameInstance::LoadAvalancheBlueprint(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceAvalancheBlueprint)
{
	if (ManagedAvalancheBlueprint && SourceAvalancheBlueprint == InSourceAvalancheBlueprint)
	{
		return false;
	}

	if (!bWorldCreated)
	{
		CreateWorld();
	}

	UAvalancheBlueprint* Source;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::LoadAsset::LoadSourceAvaBp);
		Source = InSourceAvalancheBlueprint.LoadSynchronous();
		check(Source);
	}

#if WITH_EDITORONLY_DATA
	FGuardValue_Bitfield(Source->bDuplicatingReadOnly, true);
#endif

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::LoadAsset::DupAvaBp);
		const FName AvaInstanceName = MakeUniqueObjectName(this, UAvalancheBlueprint::StaticClass(), Source->GetFName());
		ManagedAvalancheBlueprint = Cast<UAvalancheBlueprint>(StaticDuplicateObject(Source, this, AvaInstanceName, RF_Transient));
	}
	
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::LoadAsset::LoadAvaBpWorld);
		ManagedAvalancheBlueprint->SetAvalancheWorld(PlayWorld.Get());
		ManagedAvalancheBlueprint->LoadAvalancheWorld();
	}
	
	ManagedAvalancheBlueprint->UpdatePlaceholderActor();

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::LoadAsset::RebindRCP);
		ManagedAvalancheBlueprint->RegisterRemoteControlPreset();
		FAvaRemoteControlRebind::RebindUnboundEntities(ManagedAvalancheBlueprint->GetRemoteControlPreset());
	}
	
	MarkSynchronousAssetLoadingThisFrame();

	SourceAvalancheBlueprint = InSourceAvalancheBlueprint;
	return true;
}

bool UAvalancheGameInstance::BeginPlayWorld(const FAvalancheInstancePlaySettings& InWorldPlaySettings)
{
	// Make sure we don't have pending unload or stop requests left over in the game instance. 
	CancelWorldRequests();
	
	if (bWorldPlaying)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(UAvalancheGameInstance::BeginPlay);
	TRACE_BOOKMARK(TEXT("UAvalancheGameInstance::BeginPlay"));
	
	ViewportClient = NewObject<UAvalancheGameViewportClient>(GEngine, NAME_None, RF_Transient);
	check(ViewportClient);

	// Note: UGameViewportClient::Init ignores "bCreateNewAudioDevice" parameter and always create a device for the world. 
	ViewportClient->Init(*WorldContext, this);
	ViewportClient->SetRenderTarget(InWorldPlaySettings.RenderTarget);
	ViewportClient->bIsPlayInEditorViewport = false;
	FAvaViewportQualitySettings QualitySettingsMutable = InWorldPlaySettings.QualitySettings; 
	QualitySettingsMutable.Apply(ViewportClient->EngineShowFlags);
	
#if ALLOW_CONSOLE
	// Create the viewport's console.
	ViewportClient->ViewportConsole = NewObject<UConsole>(ViewportClient.Get(), GEngine->ConsoleClass);
	// register console to get all log messages
	GLog->AddOutputDevice(ViewportClient->ViewportConsole);
#endif

	TSharedPtr<SViewport> ViewportWidget;
	Viewport = MakeShareable(ViewportClient->CreateGameViewport(ViewportWidget));
	Viewport->SetInitialSize(InWorldPlaySettings.ViewportSize);

	PlayWorld->InitializeActorsForPlay(PlayWorld->URL);
	PlayWorld->BeginPlay();
	
	FCoreDelegates::OnEndFrame.AddUObject(this, &UAvalancheGameInstance::OnEndFrameTick);

	bWorldPlaying = true;
	PlayingChannelName = InWorldPlaySettings.ChannelName;
	return true;
}

void UAvalancheGameInstance::RequestEndPlayWorld(bool bForceImmediate)
{
	if (bWorldPlaying)
	{
		if (bForceImmediate)
		{
			EndPlayWorld();
		}
		else
		{
			bRequestEndPlayWorld = true;
		}
	}
}

void UAvalancheGameInstance::BeginPlayAvalancheBlueprint(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceAvalancheBlueprint, const FAvalancheInstancePlaySettings& InWorldPlaySettings)
{
	// World States: Created -> Playing
	// Asset States: Unloaded -> Loading -> Loaded -> Playing (Visible)
	const FSoftObjectPath& SourceAssetPath = InSourceAvalancheBlueprint.ToSoftObjectPath();

	if (!bWorldPlaying)
	{
		BeginPlayWorld(InWorldPlaySettings);
	}
	else
	{
		UpdateRenderTarget(InWorldPlaySettings.RenderTarget);
		UpdateSceneViewportSize(InWorldPlaySettings.ViewportSize);
	}

	// Check if the asset is loaded.
	if (SourceAvalancheBlueprint != InSourceAvalancheBlueprint)
	{
		LoadAvalancheBlueprint(InSourceAvalancheBlueprint);
	}	
	
	if (ManagedAvalancheBlueprint && SourceAvalancheBlueprint == InSourceAvalancheBlueprint)
	{
		// Verify if this can be changed on the fly.
		ManagedAvalancheBlueprint->GetViewportQualitySettings().Apply(ViewportClient->EngineShowFlags);
	
		// Old code using ava camera manager. To retire.
		constexpr bool bIsCanvasController = false;
		ViewportClient->GetCameraManager()->Init(ManagedAvalancheBlueprint->GetPlaybackObject(), bIsCanvasController);
#if WITH_EDITOR
		ViewportClient->GetCameraManager()->SetDefaultViewTarget(PlayWorld, ManagedAvalancheBlueprint->GetStartupCameraName());
#endif
	}
}

void UAvalancheGameInstance::RequestUnloadWorld(bool bForceImmediate)
{
	if (bWorldCreated)
	{
		if (bForceImmediate)
		{
			UnloadWorld();
		}
		else
		{
			bRequestUnloadWorld = true;
		}
	}
}

void UAvalancheGameInstance::CancelWorldRequests()
{
	bRequestEndPlayWorld = false;
	bRequestUnloadWorld = false;
}

void UAvalancheGameInstance::UpdateRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
	if (ViewportClient)
	{
		ViewportClient->SetRenderTarget(InRenderTarget);
	}
}

UTextureRenderTarget2D* UAvalancheGameInstance::GetRenderTarget() const
{
	if (ViewportClient)
	{
		return ViewportClient->GetRenderTarget();
	}
	return nullptr;
}

void UAvalancheGameInstance::UpdateSceneViewportSize(const FIntPoint& InViewportSize)
{
	if (Viewport.IsValid() && Viewport->GetSize() != InViewportSize)
	{
		// Note: calling either SetViewportSize or SetFixedViewportSize doesn't work
		// because ViewportWidget is null in this case. And we can't call ResizeViewport
		// either because it is private. Calling the only function we can call.		
		Viewport->UpdateViewportRHI(false, InViewportSize.X, InViewportSize.Y, EWindowMode::Type::Windowed, PF_Unknown);
	}
}

void UAvalancheGameInstance::MarkSynchronousAssetLoadingThisFrame()
{
	UE::AvalancheGameInstance::Private::MarkSynchronousAssetLoadingThisFrame();
}

void UAvalancheGameInstance::Tick(float DeltaSeconds)
{
	if (bIsTicking)
	{
		return;
	}
	TGuardValue TickGuard(bIsTicking, true);

	if (!bWorldPlaying)
	{
		return;
	}

	if (!PlayWorld)
	{
		EndPlayWorld();
		return;
	}
	
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AvalancheGame_Tick_SetDropDetail);
		ViewportClient->SetDropDetail(DeltaSeconds);
	}
	
	/*
	 * Things Removed Here because they're currently unneeded and will possibly never be
	 * TickWorldTravel
	 */

	{
		SCOPE_TIME_GUARD(TEXT("UAvalancheGameInstance::Tick - World Tick"));
		PlayWorld->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
	}

	//Removed check for Dedicated Server & Commandlet
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UGameEngine_Tick_CheckCaptures);
		
		// Only update reflection captures in game once all 'always loaded' levels have been loaded
		// This won't work with actual level streaming though
		if (PlayWorld->AreAlwaysLoadedLevelsLoaded())
		{
			// Update sky light first because it's considered direct lighting, sky diffuse will be visible in reflection capture indirect specular
			USkyLightComponent::UpdateSkyCaptureContents(PlayWorld.Get());
			UReflectionCaptureComponent::UpdateReflectionCaptureContents(PlayWorld.Get());
		}
	}

	//Rest of the stuff between above Lighting Update and GameViewport Tick removed

	{
		SCOPE_TIME_GUARD(TEXT("UAvalancheGameInstance::Tick - TickViewport"));
		QUICK_SCOPE_CYCLE_COUNTER(STAT_AvalancheGame_Tick_TickViewport);
		ViewportClient->Tick(DeltaSeconds);
	}
	
	{
		Viewport->Draw();

		if (!bIsRenderTargetReady)
		{
			if (!RenderTargetFence.IsValid())
			{
				RenderTargetFence = MakeUnique<FRenderCommandFence>();
				RenderTargetFence->BeginFence();
			}
			else
			{
				bIsRenderTargetReady = RenderTargetFence->IsFenceComplete();
				OnRenderTargetReady.Broadcast(this, PlayingChannelName);
			}
		}
	}

	if (bRequestEndPlayWorld)
	{
		EndPlayWorld();
	}
	if (bRequestUnloadWorld)
	{
		UnloadWorld();
	}
}

void UAvalancheGameInstance::UnloadWorld()
{
	if (bWorldPlaying)
	{
		EndPlayWorld();
	}

	bWorldCreated = false;
	bRequestUnloadWorld = false;

	if (IsValid(ManagedAvalancheBlueprint.Get()))
	{
		ManagedAvalancheBlueprint->UnregisterRemoteControlPreset();
	}
	SourceAvalancheBlueprint.Reset();
	ManagedAvalancheBlueprint = nullptr;
	
	if (PlayWorld)
	{
		if (FAudioDeviceHandle AudioDevice = PlayWorld->GetAudioDevice())
		{
			AudioDevice->Flush(PlayWorld, false);
		}

		// Need to do this before destroying the world context apparently.
		PlayWorld->SetShouldForceUnloadStreamingLevels(true);
		PlayWorld->FlushLevelStreaming();

		GEngine->DestroyWorldContext(PlayWorld.Get());
		PlayWorld->DestroyWorld(true);
	}

	PlayWorld = nullptr;
	WorldContext = nullptr;
}

void UAvalancheGameInstance::EndPlayWorld()
{
	bWorldPlaying = false;
	bRequestEndPlayWorld = false;

	FCoreDelegates::OnEndFrame.RemoveAll(this);

	OnEndPlay.Broadcast(this, PlayingChannelName);
	PlayingChannelName = NAME_None;

	Viewport.Reset();
	RenderTargetFence.Reset();
	bIsRenderTargetReady = false;

#if ALLOW_CONSOLE
	if (ViewportClient)
	{
		GLog->RemoveOutputDevice(ViewportClient->ViewportConsole);
	}
#endif

	ViewportClient = nullptr;
}

void UAvalancheGameInstance::OnEndFrameTick()
{
	double DeltaSeconds = FApp::GetDeltaTime();

	if (UE::AvalancheGameInstance::Private::ShouldIgnoreDeltaSecondsForCurrentFrame())
	{
		DeltaSeconds = LastDeltaSeconds;
	}
	else
	{
		LastDeltaSeconds = DeltaSeconds;
	}
	
	Tick(DeltaSeconds);
}

void UAvalancheGameInstance::OnEnginePreExit()
{
	if (PlayWorld && PlayWorld->GetAudioDevice())
	{
		// Temporarily set the GIsRequestingExit to false so that the DLLs like XAudio2Dll are not unloaded
		// for ref: FMixerPlatformXAudio2::TeardownHardware unloads the dll if Engine Exit is requested
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		TGuardValue<bool> RequestExitGuard(GIsRequestingExit, false);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		const FAudioDeviceHandle EmptyHandle;
		PlayWorld->SetAudioDevice(EmptyHandle);
	}
}

void UAvalancheGameInstance::BeginDestroy()
{
	FCoreDelegates::OnEnginePreExit.Remove(EnginePreExitHandle);
	EnginePreExitHandle.Reset();
	
	EndPlayWorld();
	UnloadWorld();
	
	Super::BeginDestroy();
}
