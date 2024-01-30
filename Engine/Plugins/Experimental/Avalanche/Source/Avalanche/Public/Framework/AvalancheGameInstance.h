// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvalancheGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "AvalancheGameInstance.generated.h"

class FRenderCommandFence;
class FSceneViewport;
class UAvalancheBlueprint;
class UTextureRenderTarget2D;
struct FAvaViewportQualitySettings;
struct FAvalancheInstanceSettings;

struct FAvalancheInstancePlaySettings
{
	const FAvalancheInstanceSettings& Settings;
	FName ChannelName;
	UTextureRenderTarget2D* RenderTarget;
	FIntPoint ViewportSize;
	const FAvaViewportQualitySettings& QualitySettings;
};

UCLASS(DisplayName = "Motion Design Game Instance")
class AVALANCHE_API UAvalancheGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new Avalanche Game instance from the given Avalanche Template Asset.
	 * Supported asset types: UAvalancheBlueprint, UWorld (Level).
	 *
	 * Remark: For Avalanche Blueprints, the instanced embedded RemoteControlPreset will be registered
	 * under the name of the outer Package Name. It is thus required that each instance be contained in
	 * a unique package to avoid stomping the registered RCPs.
	 */
	static UAvalancheGameInstance* Create(UObject* InOuter);

	// Legacy - will create and load an avalanche blueprint.
	static UAvalancheGameInstance* Create(UObject* InOuter, const TSoftObjectPtr<UAvalancheBlueprint>& InAvalancheBlueprintTemplate);
	
	/** Create the game world */
	bool CreateWorld();

	bool IsWorldCreated() const { return bWorldCreated; }

	UWorld* GetPlayWorld() const { return PlayWorld.Get(); }

	// Legacy
	bool LoadAvalancheBlueprint(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceAvalancheBlueprint);

	// Legacy
	void BeginPlayAvalancheBlueprint(const TSoftObjectPtr<UAvalancheBlueprint>& InSourceAvalancheBlueprint, const FAvalancheInstancePlaySettings& InWorldPlaySettings);
	
	bool BeginPlayWorld(const FAvalancheInstancePlaySettings& InWorldPlaySettings);

	/**
	 * The end play is normally requested to be done on the next Tick(). This is to
	 * avoid having the world destroyed while within the Tick() of that world.
	 * However, when shutting down, we need to force the end play to be done immediately because
	 * there will not a be a next Tick().
	 */
	void RequestEndPlayWorld(bool bForceImmediate);
	
	void RequestUnloadWorld(bool bForceImmediate);

	bool IsWorldPlaying() const { return bWorldPlaying; }

	/**
	 * Cancel any pending EndPlayWorld or UnloadWorld requests. 
	 */
	void CancelWorldRequests();

	/**
	* The output channels can be resized during playback, if this happens, the
	* scene viewport needs to be updated to reflect the change.
	*/
	void UpdateSceneViewportSize(const FIntPoint& InViewportSize);

	void UpdateRenderTarget(UTextureRenderTarget2D* InRenderTarget);

	UTextureRenderTarget2D* GetRenderTarget() const;

	/**
	 * Returns true if the render target has been rendered and is ready to be used for output.
	 */
	bool IsRenderTargetReady() const { return bIsRenderTargetReady;}

	/**
	 * Reset the flag so the OnRenderTargetReady event can be called again on the next render.
	 */
	void ResetRenderTargetReady() { bIsRenderTargetReady = false; }

	/**
	 * @brief Get the currently playing scene viewport.
	 * @remark Only valid within a BeginPlay()/EndPlay() scope. Null otherwise.
	 */
	TSharedPtr<FSceneViewport> GetSceneViewport() const { return Viewport; }

	/**
	 * @brief Get the currently playing game viewport client.
	 * @remark Only valid within a BeginPlay()/EndPlay() scope. Null otherwise.
	 */
	UAvalancheGameViewportClient* GetAvalancheGameViewportClient() const { return ViewportClient; }
	
	/*
	 * @return The Avalanche Blueprint used in play. Can be null if called outside play
	 */
	UAvalancheBlueprint* GetAvalancheBlueprint() const { return ManagedAvalancheBlueprint; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAvaGameInstanceEvent, UAvalancheGameInstance* /*InGameInstance*/, FName /*InChannelName*/);

	/** Event called on EndPlayWorld(). */
	static FOnAvaGameInstanceEvent& GetOnEndPlay() { return OnEndPlay; }

	/** Event called when the render target becomes ready. */
	static FOnAvaGameInstanceEvent& GetOnRenderTargetReady() { return OnRenderTargetReady; }

	/*
	 * Mark the current frame as a frame in which an asset was loaded synchronously.
	 * When an asset has been loaded synchronously (i.e. avalanche blueprint), it will cause a
	 * large delta seconds to occur on the next tick. By marking that particular frame it can be
	 * used on the next tick to clamp the delta seconds to avoid animations skipping by the
	 * amount of the sync load time.
	 */
	void MarkSynchronousAssetLoadingThisFrame();

protected:
	void Tick(float DeltaSeconds);

	void UnloadWorld();
	void EndPlayWorld();
	void OnEndFrameTick();
	void OnEnginePreExit();
	
	//~ Begin UObject
	virtual void BeginDestroy() override;
	//~ End UObject

	/*
	 * A Soft Ptr to the Source Avalanche (i.e. the actual asset).
	 * It's a Soft Ptr because it can become stale if it's only a Weak Object Ptr
	 * and we want to reload it if that happens
	 */
	TSoftObjectPtr<UAvalancheBlueprint> SourceAvalancheBlueprint;

	/*
	 * The Managed Avalanche, a Duplication of the Source Avalanche when Begin Play was called.
	 * This shouldn't be GCd while in play, as it contains Animation and other data used during play.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAvalancheBlueprint> ManagedAvalancheBlueprint;

	UPROPERTY(Transient)
	TObjectPtr<UWorld> PlayWorld;

	UPROPERTY(Transient)
	TObjectPtr<UAvalancheGameViewportClient> ViewportClient;

	TSharedPtr<FSceneViewport> Viewport;

	/** Channel this game instance is playing on. */
	FName PlayingChannelName;

	bool bWorldCreated = false;
	bool bRequestUnloadWorld = false;
	bool bWorldPlaying = false;
	bool bRequestEndPlayWorld = false;

	/** Tick reentrancy guard. */
	bool bIsTicking = false;
	
	double LastDeltaSeconds = 0.0;

	/** Render command fence to ensure the render target has been rendered. */
	TUniquePtr<FRenderCommandFence> RenderTargetFence;
	bool bIsRenderTargetReady = false;

	/** Handle to delegate for EnginePreExit event. */
	FDelegateHandle EnginePreExitHandle;

	static FOnAvaGameInstanceEvent OnEndPlay;
	static FOnAvaGameInstanceEvent OnRenderTargetReady;
};
