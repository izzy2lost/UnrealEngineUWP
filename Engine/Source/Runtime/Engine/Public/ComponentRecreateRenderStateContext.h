// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Components/ActorComponent.h"
#include "SceneInterface.h"

/** Destroys render state for a component and then recreates it when this object is destroyed */
class FComponentRecreateRenderStateContext
{
private:
	/** Pointer to component we are recreating render state for */
	UActorComponent* Component = nullptr;

	TSet<FSceneInterface*>* ScenesToUpdateAllPrimitiveSceneInfos = nullptr;

public:
	FComponentRecreateRenderStateContext(UActorComponent* InComponent, TSet<FSceneInterface*>* InScenesToUpdateAllPrimitiveSceneInfos = nullptr)
		: ScenesToUpdateAllPrimitiveSceneInfos(InScenesToUpdateAllPrimitiveSceneInfos)
	{
		check(InComponent);
		checkf(!InComponent->IsUnreachable(), TEXT("%s"), *InComponent->GetFullName());

		if (InComponent->IsRegistered() && InComponent->IsRenderStateCreated())
		{
			InComponent->DestroyRenderState_Concurrent();
			Component = InComponent;

			UpdateAllPrimitiveSceneInfosForSingleComponent(InComponent, ScenesToUpdateAllPrimitiveSceneInfos);
		}
		else
		{
			Component = nullptr;
		}
	}

	FComponentRecreateRenderStateContext(const FComponentRecreateRenderStateContext&) = delete;
	FComponentRecreateRenderStateContext& operator=(const FComponentRecreateRenderStateContext&) = delete;
	
	FComponentRecreateRenderStateContext(FComponentRecreateRenderStateContext&& Other)
		: Component(Other.Component)
		, ScenesToUpdateAllPrimitiveSceneInfos(Other.ScenesToUpdateAllPrimitiveSceneInfos)
	{
		Other.Component = nullptr;
		Other.ScenesToUpdateAllPrimitiveSceneInfos = nullptr;
	}

	FComponentRecreateRenderStateContext& operator=(FComponentRecreateRenderStateContext&& Other)
	{
		Component = Other.Component;
		ScenesToUpdateAllPrimitiveSceneInfos = Other.ScenesToUpdateAllPrimitiveSceneInfos;
		Other.Component = nullptr;
		Other.ScenesToUpdateAllPrimitiveSceneInfos = nullptr;
		return *this;
	}

	~FComponentRecreateRenderStateContext()
	{
		if (Component && !Component->IsRenderStateCreated() && Component->IsRegistered())
		{
			Component->CreateRenderState_Concurrent(nullptr);

			UpdateAllPrimitiveSceneInfosForSingleComponent(Component, ScenesToUpdateAllPrimitiveSceneInfos);
		}
	}
};

/** Destroys render states for all components or for a provided list of components and then recreates them when this object is destroyed */
class FGlobalComponentRecreateRenderStateContext
{
public:
	/** 
	* Initialization constructor. 
	*/
	ENGINE_API FGlobalComponentRecreateRenderStateContext();

	/** 
	* Initialization constructor for a provided component list. 
	*/
	ENGINE_API FGlobalComponentRecreateRenderStateContext(const TArray<UActorComponent*>& InComponents);


	/** Destructor */
	ENGINE_API ~FGlobalComponentRecreateRenderStateContext();

private:
	/** The recreate contexts for the individual components. */
	TArray<FComponentRecreateRenderStateContext> ComponentContexts;

	TSet<FSceneInterface*> ScenesToUpdateAllPrimitiveSceneInfos;

	void UpdateAllPrimitiveSceneInfos();
};
