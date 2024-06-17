// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowSimulationContext.h"

class UDataflow;
class UChaosCacheCollection;
class AChaosCacheManager;
class UDataflowBaseContent;

namespace Dataflow
{
	/** Evaluate the simulation graph given a simulation context and a simulation flag */
	DATAFLOWSIMULATION_API void EvaluateSimulationGraph(const TObjectPtr<UDataflow>& SimulationGraph,
		const TSharedPtr<Dataflow::FDataflowSimulationContext>& SimulationContext, const float DeltaTime, const float SimulationTime);

	/** Check if the simulation cache nodes have changed to trigger a reset */
	DATAFLOWSIMULATION_API bool ShouldResetWorld(const TObjectPtr<UDataflow>& SimulationGraph,
		const TObjectPtr<UWorld>& SimulationWorld, Dataflow::FTimestamp& LastTimeStamp);

	/** Spawn an actor given a class type and attach it to the cache manager */
	DATAFLOWSIMULATION_API TObjectPtr<AActor> SpawnSimulatedActor(const TSubclassOf<AActor>& ActorClass,
		const TObjectPtr<AChaosCacheManager>& CacheManager, const TObjectPtr<UChaosCacheCollection>& CacheCollection,
		const bool bIsRecording, const TObjectPtr<UDataflowBaseContent>& DataflowContent);

	/** Setup the skelmesh animations to be used in the scene/generator */
	DATAFLOWSIMULATION_API void SetupSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor);

	/** Update the skelmesh animation at some point in time */
	DATAFLOWSIMULATION_API void UpdateSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor, const float SimulationTime);

	/** Start the skelmesh animation */
	DATAFLOWSIMULATION_API void StartSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor);

	/** Pause the skelmesh animation */
	DATAFLOWSIMULATION_API void PauseSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor);
	
	/** Step the skelmesh animation */
	DATAFLOWSIMULATION_API void StepSkeletonAnimation(const TObjectPtr<AActor>& PreviewActor);
};


