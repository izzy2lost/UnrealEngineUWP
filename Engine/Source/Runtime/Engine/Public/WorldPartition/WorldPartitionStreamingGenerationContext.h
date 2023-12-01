// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "OverrideVoidReturnInvoker.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"

class FStreamingGenerationActorDescViewMap;
class FWorldPartitionStreamingGenerator;
class FStreamingGenerationActorDescCollection;
class FWorldPartitionActorDescView;
class UActorDescContainer;
class UDataLayerInstance;
struct FWorldPartitionRuntimeContainerResolver;

#if WITH_EDITOR
class IStreamingGenerationContext
{
public:
	virtual ~IStreamingGenerationContext()
	{}

	/**
	 * An actor set represents a group of actors that needs to be part of the same streaming cell, because they have hard references between them.
	 */
	struct FActorSet
	{
		TArray<FGuid> Actors;
	};

	/**
	 * An actor set container represents the list of actor sets in an actor container, e.g. a level instance.
	 */
	struct FActorSetContainer
	{
		FActorSetContainer()
			: ActorDescViewMap(nullptr)
			, ActorDescCollection(nullptr)
		{}

		// Non-copyable
		FActorSetContainer(const FActorSetContainer&) = delete;
		FActorSetContainer& operator=(const FActorSetContainer&) = delete;

		const FStreamingGenerationActorDescViewMap* ActorDescViewMap;
		const FStreamingGenerationActorDescCollection* ActorDescCollection; // Only used by UWorldPartitionRuntimeSpatialHash::SetupHLODActors
		TArray<TUniquePtr<FActorSet>> ActorSets;
	};

	/**
	 * An actor set instance is an actual intance of an actor set in the world.
	 */
	struct FActorSetInstance
	{
		FBox Bounds;
		FName RuntimeGrid;
		bool bIsSpatiallyLoaded;
		TArray<const UDataLayerInstance*> DataLayers;
		FGuid ContentBundleID;
		const FActorSetContainer* ContainerInstance;
		FActorContainerID ContainerID;
		FTransform Transform;
		const FActorSet* ActorSet;
		const TSet<FGuid>* FilteredActors;

		template <typename Func>
		void ForEachActor(Func InFunc) const
		{
			TOverrideVoidReturnInvoker Invoker(true, InFunc);

			for (const FGuid& ActorGuid : ActorSet->Actors)
			{
				if (!FilteredActors || !FilteredActors->Contains(ActorGuid))
				{
					if (!Invoker(ActorGuid))
					{
						break;
					}
				}
			}
		}
	};

	/**
	 * An actor instance represents a single instanced actor in the world.
	 */
	struct FActorInstance
	{
		FActorInstance(const FGuid& InActorGuid, const FActorSetInstance* InActorSetInstance)
			: ActorGuid(InActorGuid)
			, ActorSetInstance(InActorSetInstance)
		{}

		FGuid ActorGuid;
		const FActorSetInstance* ActorSetInstance;

		ENGINE_API const FWorldPartitionActorDescView& GetActorDescView() const;
		ENGINE_API const FActorContainerID& GetContainerID() const;
		ENGINE_API const FTransform& GetTransform() const;
		ENGINE_API const FBox GetBounds() const;
	};

	virtual FBox GetWorldBounds() const = 0;
	virtual const FActorSetContainer* GetMainWorldContainer() const = 0;
	virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const = 0;
	virtual void ForEachActorSetContainer(TFunctionRef<void(const FActorSetContainer&)> Func) const = 0;
};

class FStreamingGenerationContextProxy : public IStreamingGenerationContext
{
public:
	FStreamingGenerationContextProxy(const IStreamingGenerationContext* InSourceContext)
		: SourceContext(InSourceContext)
	{}

	virtual FBox GetWorldBounds() const override
	{
		return SourceContext->GetWorldBounds();
	}

	virtual const FActorSetContainer* GetMainWorldContainer() const override
	{
		return SourceContext->GetMainWorldContainer();
	}

	virtual void ForEachActorSetInstance(TFunctionRef<void(const FActorSetInstance&)> Func) const override
	{
		SourceContext->ForEachActorSetInstance(Func);
	}

	virtual void ForEachActorSetContainer(TFunctionRef<void(const FActorSetContainer&)> Func) const override
	{
		SourceContext->ForEachActorSetContainer(Func);
	}

protected:
	const IStreamingGenerationContext* SourceContext;
};
#endif