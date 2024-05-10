// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosDebugDraw/ChaosDDContext.h"
#include "ChaosDebugDraw/ChaosDDTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "ChaosDebugDrawSubsystem.generated.h"

class UWorld;
class FChaosDDRenderer;

UCLASS(MinimalAPI)
class UChaosDebugDrawSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	CHAOSSOLVERENGINE_API const ChaosDD::Private::FChaosDDScenePtr& GetDebugDrawScene() const { return CDDScene; }

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override final;
	virtual void PostInitialize() override final;
	virtual void Deinitialize() override final;

	static void Startup();
	static void Shutdown();

protected:
	void OnWorldTickStart(ELevelTick TickType, float Dt);
	void OnWorldTickEnd(ELevelTick TickType, float Dt);
	void RenderScene();
	void RenderScene(FChaosDDRenderer& Renderer, const ChaosDD::Private::FChaosDDScenePtr& Scene);
	void RenderFrame(FChaosDDRenderer& Renderer, const ChaosDD::Private::FChaosDDFramePtr& Frame);

	ChaosDD::Private::FChaosDDScenePtr CDDScene;
	ChaosDD::Private::FChaosDDTimelinePtr CDDWorldTimeline;
	ChaosDD::Private::FChaosDDTimelineContext CDDWorldTimelineContext;

	static void StaticOnWorldTickStart(UWorld* World, ELevelTick TickType, float Dt);
	static void StaticOnWorldTickEnd(UWorld* World, ELevelTick TickType, float Dt);
	static FDelegateHandle OnTickWorldStartDelegate;
	static FDelegateHandle OnTickWorldEndDelegate;
};
