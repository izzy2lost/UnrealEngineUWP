// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectPtr.h"
#include "TickableEditorObject.h"
#include "Logging/LogMacros.h"
#include "Misc/AsyncTaskNotification.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowSimulationUtils.h"

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

		/** Simulation world */
		UWorld* SimulationWorld = nullptr;

		/** Boolean to check if we are running the task in the background */
		bool bBackgroundTask = true;
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
        bool AllocateSimulationResource(const FVector2f& TimeRange, const int32 FrameRate,
        	const TObjectPtr<UChaosCacheCollection>& CacheAsset, const TSubclassOf<AActor>& ActorClass,
        	const TObjectPtr<UDataflowBaseContent>& DataflowContent);

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

		/** Temporary cache manager created to run the simulation */
		TObjectPtr<AChaosCacheManager> CacheManager = nullptr;

		/** Temporary cache manager created to run the simulation */
		TObjectPtr<AActor> PreviewActor = nullptr;
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

		/** Set the frame rate for cache recording */
		void SetFrameRate(const int32 InFrameRate);

		/** Set the time range for cache recording */
		void SetTimeRange(const FVector2f& InTimeRange);

		/** Set the cache asset for cache recording */
		void SetCacheAsset(const TObjectPtr<UChaosCacheCollection>& InCacheAsset);

		/** Set the actor class for cache recording */
		void SetActorClass(const TSubclassOf<AActor>& InActorClass);

		/** Set the dataflow content */
		void SetDataflowContent(const TObjectPtr<UDataflowBaseContent>& InDataflowContent);

		/** Set the background task boolean */
		void SetBackgroundTask(const bool bInBackgroundTask);

		/** Enqueue a generator action to be processed on the async thread */
		void RequestGeneratorAction(EDataflowGeneratorActions Action);
		
	private:

		/** Start generating the simulation */
		void StartGenerateSimulation();
		
		/** Tick the generated simulation */
		void TickGenerateSimulation();

		/** Free the task resource used while generating the simulation */
		void FreeTaskResource(bool bCancelled);

		/** Cache asset to store the caches */
		TObjectPtr<UChaosCacheCollection> CacheAsset = nullptr;

		/** Frame rate used to record the cache */
		int32 FrameRate = 30;

		/** Time range used to generate the cache */
		FVector2f TimeRange = FVector2f(0.0f, 5.0f);

		/** Boolean to control if the task is going to be run in the background */
		bool bBackgroundTask = false;

		/** Chaos cache manager BP class to be spawned */
		TSubclassOf<AActor> ActorClass;

		/** Dataflow content */
		TObjectPtr<UDataflowBaseContent> DataflowContent;

		/** Pending action to be send to the async thread */
		EDataflowGeneratorActions PendingAction = EDataflowGeneratorActions::NoAction;

		/** Task manager to run the async tasks */
		TSharedPtr<FDataflowTaskManager> TaskManager = nullptr;
	};
};


