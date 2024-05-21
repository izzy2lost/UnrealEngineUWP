// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"
#include "TickableEditorObject.h"
#include "Logging/LogMacros.h"
#include "Misc/AsyncTaskNotification.h"
#include "Dataflow/DataflowContent.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDataflowSimulationGenerator, Log, All);

class UChaosCacheCollection;
class AChaosCacheManager;

namespace Dataflow
{
	/** Simulation Task to be run on the async thread */
	class FDataflowSimulationTask : public FNonAbandonableTask
	{
		public:
		FDataflowSimulationTask()
		{}

		/** Run the simulation */
		void DoWork();

		/** Can abandon check */
		bool CanAbandon()
		{
			return true;
		}

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(TTaskRunner, STATGROUP_ThreadPoolAsyncTasks);
		}
	
		/** Task Manager */
		TSharedPtr<struct FDataflowTaskManager> TaskManager = nullptr;
		
		/** Simulation delta time */
		float DeltaTime = 0.0f;

		/** Simulation min time */
		float MinTime = TNumericLimits<float>::Max();

		/** Simulation max time */
		float MaxTime = TNumericLimits<float>::Lowest();

		/** Simulation graph to be executed */
		TObjectPtr<UDataflow> SimulationGraph;

		/** Simulation  context used during graph evaluation */
		TSharedPtr<Dataflow::FSimulationContext> SimulationContext;
	};

	/** Async simulation resource that will be used while simulating */
	struct FDataflowSimulationResource
	{
		/** Number of simulated frames */
		std::atomic<int32>* NumSimulatedFrames = nullptr;

		/** Async cancel boolean */
		std::atomic<bool>* bCancelled = nullptr;

		/** Check if the simulation is canceled or not */
		bool IsCancelled() const
		{
			return !bCancelled || bCancelled->load();
		}
		
		/** Finish simulating the current frame */
		void FinishFrame()
		{
			if (NumSimulatedFrames)
			{
				++(*NumSimulatedFrames);
			}
		}
	};

	/** Simulation Task manager */
	struct FDataflowTaskManager
	{
		/** Simulation resource */
		TSharedPtr<FDataflowSimulationResource> SimulationResource;

		/** Simulation task */
		TUniquePtr<FAsyncTask<FDataflowSimulationTask>> SimulationTask;

		/** Async notification*/
		TUniquePtr<FAsyncTaskNotification> AsyncNotification;

		/** Number of frames to simulate */
		int32 NumFrames;

		/** Start time of the simulation */
		FDateTime StartTime;

		/** Last updated time */
		FDateTime LastUpdateTime;

		/** Allocate the simulation resource from the properties */
        bool AllocateSimulationResource(const TObjectPtr<UDataflow> SimulationGraph, const int32 SamplingRate, const TObjectPtr<UChaosCacheCollection>& CacheCollection);

        /** Free the simulation resource */
        void FreeSimulationResource();

        /** Cancel the simulation generation */
        void CancelSimulationGeneration();

		/** Number of simulated frames */
		std::atomic<int32> NumSimulatedFrames = 0;
		
		/** Boolean to check if the simulation has been cancelled */
		std::atomic<bool> bCancelled = false;
		
		/** Temporary world created to run the simulation */
		UWorld* SimulationWorld = nullptr;
	};

	/** Enum for all the generator actions */
	enum class EDataflowGeneratorActions
	{
		NoAction,
		StartGenerate,
		TickGenerate
	};

	/** Dataflow simulation generator */
	class  FDataflowSimulationGenerator : public FTickableEditorObject
	{
	public:
		FDataflowSimulationGenerator();
		virtual ~FDataflowSimulationGenerator();

		//~ Begin FTickableEditorObject Interface
		virtual void Tick(float DeltaTime) override;
		virtual TStatId GetStatId() const override;
		virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
		//~ End FTickableEditorObject Interface

		/** Set the simulation graph to be executed on the async thread */
		void SetSimulationGraph(const TObjectPtr<UDataflow>& DataflowGraph);

		/** Set the sampling rate for cache recording */
		void SetSamplingRate(const int32 FrameRate);

		/** Set the cache collection for cache recording */
		void SetCacheCollection(const TObjectPtr<UChaosCacheCollection>&);

		/** Enqueue a generator action to be processed on the async thread */
		void RequestGeneratorAction(EDataflowGeneratorActions Action);
		
	private:

		/** Start generating the simulation */
		void StartGenerateSimulation();
		
		/** Tick the generated simulation */
		void TickGenerateSimulation();

		/** Free the task resource used while generating the simulation */
		void FreeTaskResource(bool bCancelled);

		/** Graph to be used to generate the simulation output */
		TObjectPtr<UDataflow> SimulationGraph = nullptr;

		/** Cache collection to store the caches */
		TObjectPtr<UChaosCacheCollection> CacheCollection = nullptr;

		/** Sampling rate used to record the cache */
		int32 SamplingRate = 30;

		/** Pending action to be send to the async thread */
		EDataflowGeneratorActions PendingAction = EDataflowGeneratorActions::NoAction;

		/** Task manager to run the async tasks */
		TSharedPtr<FDataflowTaskManager> TaskManager = nullptr;
	};

	/** Evaluate the simulation graph given a simulation context and a simulation flag */
	void EvaluateSimulationGraph(const TObjectPtr<UDataflow>& SimulationGraph,
		const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext, const float DeltaTime,
		const float SimulationTime, const ESimulationFlags SimulationFlags);

	/** Extract the time range, number of frames and duration of the cache sequence */
	float GetCacheDuration(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext,
		const int32 SamplingRate, float& MinTime, float& MaxTime, int32& NumFrames);

	/** Check if the simulation cache nodes have changed to trigger a reset */
	bool ShouldResetWorld(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext, Dataflow::FTimestamp& LastTimeStamp);

	/** Update the animation nodes given a context and an animation time */
	void UpdateAnimationNodes(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext, const float AnimationTime);

	/** clean the context (unregister componentds, clesn cache, reset context)*/
	void CleanSimulationContext(TSharedPtr<Dataflow::FSimulationContext>& SimulationContext);

	/** Build simulaiton context */
	void BuildSimulationContext(const TObjectPtr<UObject>& SimulationOwner, const TObjectPtr<UDataflow>& SimulationGraph, const TObjectPtr<AChaosCacheManager>& CacheManager,
		const ECachingMode CachingMode, TSharedPtr<Dataflow::FSimulationContext>& SimulationContext);
};


