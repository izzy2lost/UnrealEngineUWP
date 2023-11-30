// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneExtensions.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "RenderGraphBuilder.h"
#include "Misc/ScopeLock.h"

FSceneExtensionRegistry* FSceneExtensionRegistry::GlobalRegistry = nullptr;

void FSceneExtensionRegistry::InitRegistry()
{
	if (!GlobalRegistry)
	{
		GlobalRegistry = new FSceneExtensionRegistry;
	}
}

void FSceneExtensionRegistry::Register(ISceneExtensionFactory& Factory)
{
	Factory.ExtensionID = Factories.Num();
	Factories.Add(&Factory);
}

TArray<ISceneExtension*> FSceneExtensionRegistry::CreateExtensions(FScene& Scene)
{
	TArray<ISceneExtension*> Extensions;
	Extensions.Reserve(Factories.Num());

	int32 ExtensionID = 0;
	for (auto* Factory : Factories)
	{
		checkSlow(Factory->GetExtensionID() == ExtensionID); // sanity check
		if (auto Extension = Factory->CreateInstance(Scene))
		{
			Extensions.Add(Extension);
		}
		++ExtensionID;
	}

	return Extensions;
}

void FSceneExtensions::Init(FScene& Scene)
{
	Extensions = FSceneExtensionRegistry::Get().CreateExtensions(Scene);
	for (auto Extension : Extensions)
	{
		Extension->InitExtension(Scene);
	}
}

void FSceneExtensions::Reset()
{
	for (auto Extension : Extensions)
	{
		delete Extension;
	}
	Extensions.Reset();
}

void FSceneExtensions::CreateUpdaters(FUpdaterList& OutUpdaters)
{
	OutUpdaters.Reserve(OutUpdaters.Num() + Extensions.Num());
	for (auto Extension : Extensions)
	{
		if (auto Updater = Extension->CreateUpdater())
		{
			OutUpdaters.Add(Updater);
		}
	}	
}

void FSceneExtensions::CreateRenderers(FRendererList& OutRenderers)
{
	OutRenderers.Reserve(OutRenderers.Num() + Extensions.Num());
	for (auto Extension : Extensions)
	{
		if (auto Renderer = Extension->CreateRenderer())
		{
			OutRenderers.Add(Renderer);
		}
	}
}

void FSceneExtensionsUpdater::Begin(FScene& InScene)
{
	checkf(!IsUpdating(), TEXT("Detected FSceneExtensionsUpdater Begin() without matching End()"));
	
	Scene = &InScene;
	Scene->SceneExtensions.CreateUpdaters(Updaters);
	for (auto Updater : Updaters)
	{
		Updater->Begin(InScene);
	}
}

void FSceneExtensionsUpdater::End()
{
	for (auto Updater : Updaters)
	{
		Updater->End();
		delete Updater;
	}
	Updaters.Reset();
	Scene = nullptr;
}

void FSceneExtensionsRenderer::Begin(FSceneRendererBase& InSceneRenderer)
{
	checkf(!IsRendering(), TEXT("Detected FSceneExtensionsRenderer Begin() without matching End()"));

	SceneRenderer = &InSceneRenderer;
	check(SceneRenderer->Scene != nullptr);

	SceneRenderer->Scene->SceneExtensions.CreateRenderers(Renderers);
	for (auto Renderer : Renderers)
	{
		Renderer->Begin(InSceneRenderer);
	}
}

void FSceneExtensionsRenderer::End()
{
	for (auto Renderer : Renderers)
	{
		Renderer->End();
		delete Renderer;
	}
	Renderers.Reset();
	SceneRenderer = nullptr;
}
