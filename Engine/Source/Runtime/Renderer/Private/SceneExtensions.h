// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Tasks/Task.h"

class FScene;
class FSceneRendererBase;
class FRDGBuilder;
class FSceneUniformBuffer;
class FScenePreUpdateChangeSet;
class FScenePostUpdateChangeSet;
class FSceneExtensionRegistry;
class ISceneExtensionUpdater;
class ISceneExtensionRenderer;

/** Abstract interface for an extension to the persistent data of a scene */
class ISceneExtension
{
public:
	// default fallback static method that can be overridden in child classes to predicate the creation of the extension
	static bool ShouldCreateExtension(FScene& Scene) { return true; }

	virtual ~ISceneExtension() {}

	virtual void InitExtension(FScene& InScene) = 0;
	virtual ISceneExtensionUpdater* CreateUpdater() { return nullptr; }
	virtual ISceneExtensionRenderer* CreateRenderer() { return nullptr; }
};

/** Abstract interface to receive change sets to perform updates based on scene primitive data. */
class ISceneExtensionUpdater
{
public:
	virtual ~ISceneExtensionUpdater() {}

	virtual void Begin(FScene& InScene) {}
	virtual void End() {}
	virtual void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet) {}
	virtual void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet) {}
	virtual void PostGPUSceneUpdate(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniforms) {}
};

/** Abstract interface for an extension to the scene renderer */
class ISceneExtensionRenderer
{
public:
	virtual ~ISceneExtensionRenderer() {}

	virtual void Begin(FSceneRendererBase& InRenderer) {}
	virtual void End() {}
	virtual void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniforms) {}
	virtual void PreRender(FRDGBuilder& GraphBuilder) {}
	virtual void PostRender(FRDGBuilder& GraphBuilder) {}
};

/** Abstract interface for creating an instance of a scene extension */
class ISceneExtensionFactory
{
	friend class FSceneExtensionRegistry;

public:
	virtual ~ISceneExtensionFactory() {}
	virtual ISceneExtension* CreateInstance(FScene& Scene) = 0;

	const int32 GetExtensionID() const { return ExtensionID; }

private:
	int32 ExtensionID = INDEX_NONE;
};

/** Static class used to store a global registry of extension types */
class FSceneExtensionRegistry
{
public:
	static FSceneExtensionRegistry& Get()
	{
		InitRegistry();
		return *GlobalRegistry;
	}

	void Register(ISceneExtensionFactory& Factory);
	TArray<ISceneExtension*> CreateExtensions(FScene& Scene);

private:
	static void InitRegistry();

	TArray<ISceneExtensionFactory*> Factories;

	static FSceneExtensionRegistry* GlobalRegistry;
};

/** A collection of scene extensions */
class FSceneExtensions
{	
public:
	using FUpdaterList = TArray<ISceneExtensionUpdater*, FSceneRenderingArrayAllocator>;
	using FRendererList = TArray<ISceneExtensionRenderer*, FSceneRenderingArrayAllocator>;

	~FSceneExtensions() { Reset(); }

	void Init(FScene& Scene);
	void Reset();
	void CreateUpdaters(FUpdaterList& OutUpdaters);
	void CreateRenderers(FRendererList& OutRenderers);

	template<typename TDerivedExtension>
	TDerivedExtension* GetExtension()
	{
		const int32 Index = TDerivedExtension::GetExtensionID();
		if (ensure(Extensions.IsValidIndex(Index)))
		{
			return static_cast<TDerivedExtension*>(Extensions[Index]);
		}
		return nullptr;
	}

	template<typename TDerivedExtension>
	const TDerivedExtension* GetExtension() const
	{
		return const_cast<FSceneExtensions*>(this)->GetExtension<TDerivedExtension>();
	}
	
	template<typename TDerivedExtension>
	TDerivedExtension& GetExtensionChecked()
	{
		TDerivedExtension* Extension = this->GetExtension<TDerivedExtension>();
		check(Extension != nullptr);
		return *Extension;
	}
	
	template<typename TDerivedExtension>
	const TDerivedExtension& GetExtensionChecked() const
	{
		return const_cast<FSceneExtensions*>(this)->GetExtensionChecked<TDerivedExtension>();
	}

	template<typename TFunc>
	void ForEachExtension(const TFunc& F)
	{
		for(auto* Ext : Extensions)
		{
			F(Ext);
		}
	}

private:
	TArray<ISceneExtension*> Extensions;
};

/** Performs updates for the given scene extensions */
class FSceneExtensionsUpdater
{
	friend class FSceneExtensions;

public:
	FSceneExtensionsUpdater() {}
	explicit FSceneExtensionsUpdater(FScene& InScene) { Begin(InScene); }
	~FSceneExtensionsUpdater() { End(); }

	void Begin(FScene& InScene);
	void End();
	bool IsUpdating() const { return Scene != nullptr; }

	void PreSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePreUpdateChangeSet& ChangeSet)
	{
		for (auto Updater : Updaters) { Updater->PreSceneUpdate(GraphBuilder, ChangeSet); }
	}

	void PostSceneUpdate(FRDGBuilder& GraphBuilder, const FScenePostUpdateChangeSet& ChangeSet)
	{
		for (auto Updater : Updaters) { Updater->PostSceneUpdate(GraphBuilder, ChangeSet); }
	}

	void PostGPUSceneUpdate(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniforms)
	{
		for (auto Updater : Updaters) { Updater->PostGPUSceneUpdate(GraphBuilder, SceneUniforms); }
	}

private:
	FScene* Scene = nullptr;
	FSceneExtensions::FUpdaterList Updaters;
};

/** Performs rendering for the given scene extensions */
class FSceneExtensionsRenderer
{
	friend class FSceneExtensions;

public:
	FSceneExtensionsRenderer() {}
	FSceneExtensionsRenderer(FSceneRendererBase& InSceneRenderer) { Begin(InSceneRenderer); }
	~FSceneExtensionsRenderer() { End(); }
	
	void Begin(FSceneRendererBase& InSceneRenderer);
	void End();
	bool IsRendering() const { return SceneRenderer != nullptr; }

	void UpdateSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer& SceneUniforms)
	{
		for (auto Renderer : Renderers) { Renderer->UpdateSceneUniformBuffer(GraphBuilder, SceneUniforms); }
	}
	
	void PreRender(FRDGBuilder& GraphBuilder)
	{
		for (auto Renderer : Renderers) { Renderer->PreRender(GraphBuilder); }
	}
	
	void PostRender(FRDGBuilder& GraphBuilder)
	{
		for (auto Renderer : Renderers) { Renderer->PostRender(GraphBuilder); }
	}

private:
	FSceneRendererBase* SceneRenderer = nullptr;
	FSceneExtensions::FRendererList Renderers;
};

/** Helper to automatically register/unregister a factory implementation for a given ISceneExtension implementation */
template<typename TDerivedExtension>
class TSceneExtensionRegistration : public ISceneExtensionFactory
{
public:
	TSceneExtensionRegistration()
	{
		FSceneExtensionRegistry::Get().Register(*this);
	}

	virtual ~TSceneExtensionRegistration() {}

	virtual ISceneExtension* CreateInstance(FScene& Scene) override
	{
		if (!TDerivedExtension::ShouldCreateExtension(Scene))
		{
			return nullptr;
		}
		return new TDerivedExtension();
	}
};

/** Use this macros in the class definition of your extension. */
#define DECLARE_SCENE_EXTENSION(ClassName) \
	public: \
		static int32 GetExtensionID() { return ExtensionRegistration.GetExtensionID(); } \
	private: \
		static TSceneExtensionRegistration<ClassName> ExtensionRegistration

/** Use this macros in the implementation source file of your extension. */
#define IMPLEMENT_SCENE_EXTENSION(ClassName) \
	TSceneExtensionRegistration<ClassName> ClassName::ExtensionRegistration
