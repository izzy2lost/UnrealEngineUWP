// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSimulationGenerator.h"

#include "Engine/World.h"
#include "Chaos/CacheManagerActor.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowSolverNodes.h"
#include "Misc/AsyncTaskNotification.h" 

DEFINE_LOG_CATEGORY(LogDataflowSimulationGenerator);

#define LOCTEXT_NAMESPACE "DataflowSimulationGenerator"

namespace Dataflow
{
	float GetCacheDuration(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext, const int32 SamplingRate, float& MinTime, float& MaxTime, int32& NumFrames)
	{
		float CacheDuration = 0.0f;
		if(SimulationContext.IsValid())
		{
			SimulationContext->SetSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation);
		}
		if(const TSharedPtr<Dataflow::FGraph> DataflowGraph = SimulationGraph->GetDataflow())
		{
			MinTime = TNumericLimits<float>::Max();
			MaxTime = TNumericLimits<float>::Lowest();
			for(const TSharedPtr<FDataflowNode>& CacheNode : DataflowGraph->GetFilteredNodes(FDataflowCacheNode::StaticType()))
			{
				const FVector2f NodeRange = StaticCastSharedPtr<FDataflowCacheNode>(CacheNode)->GetTimeRange(*SimulationContext.Get());
				MinTime = FMath::Min(MinTime, NodeRange[0]);
				MaxTime = FMath::Max(MaxTime, NodeRange[1]);
			}
			NumFrames = (MaxTime > MinTime) ? FMath::Floor((MaxTime - MinTime) * SamplingRate) : 0;
			CacheDuration = ( MaxTime > MinTime) ? (MaxTime - MinTime) : 0.0f;
		}
		if(SimulationContext.IsValid())
		{
			SimulationContext->UnsetSimulationFlag(Dataflow::ESimulationFlags::SetupSimulation);
		}
		return CacheDuration;
	}

	bool ShouldResetWorld(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext, Dataflow::FTimestamp& LastTimeStamp)
	{
		if(const TSharedPtr<Dataflow::FGraph> DataflowGraph = SimulationGraph->GetDataflow())
		{
			for(const TSharedPtr<FDataflowNode>& CacheNode : DataflowGraph->GetFilteredNodes(FDataflowCacheNode::StaticType()))
			{
				if(CacheNode->GetTimestamp() >= LastTimeStamp)
				{
					LastTimeStamp = SimulationContext->GetTimestamp();
					return true;
				}
			}
		}
		return false;
	}

	void UpdateAnimationNodes(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext, const float AnimationTime)
	{
		if(const TSharedPtr<Dataflow::FGraph> DataflowGraph = SimulationGraph->GetDataflow())
		{
			for(const TSharedPtr<FDataflowNode>& AnimationNode : DataflowGraph->GetFilteredNodes(FDataflowAnimationNode::StaticType()))
			{
				StaticCastSharedPtr<FDataflowAnimationNode>(AnimationNode)->SetAnimationTime(*SimulationContext.Get(), AnimationTime);
			}
		}
	}
	
	void EvaluateSimulationGraph(const TObjectPtr<UDataflow>& SimulationGraph, const TSharedPtr<Dataflow::FSimulationContext>& SimulationContext,
		const float DeltaTime, const float SimulationTime, const ESimulationFlags SimulationFlags)
	{
		if(SimulationContext.IsValid())
		{
			SimulationContext->SetSimulationFlag(SimulationFlags);
			if(SimulationContext->HasSimulationFlag(ESimulationFlags::UpdateSimulation))
			{
				SimulationContext->SetTimingInfos(DeltaTime, SimulationTime);
			}
		}

		if(SimulationGraph)
		{
			if(const TSharedPtr<Dataflow::FGraph> DataflowGraph = SimulationGraph->GetDataflow())
			{
				if(SimulationContext->HasSimulationFlag(ESimulationFlags::UpdateSimulation))
				{
					// Invalidation of all the simulation nodes that are always dirty
					for(const TSharedPtr<FDataflowNode>& SimulationNode : DataflowGraph->GetFilteredNodes(FDataflowInvalidNode::StaticType()))
					{
						SimulationNode->Invalidate();
					}
				}

				// Pull the graph evaluation from the cache nodes
				for(const TSharedPtr<FDataflowNode>& CacheNode : DataflowGraph->GetFilteredNodes(FDataflowCacheNode::StaticType()))
				{
					SimulationContext->Evaluate(CacheNode.Get(), nullptr);
				}
			}
		}
		if(SimulationContext.IsValid())
		{
			SimulationContext->UnsetSimulationFlag(SimulationFlags);
		}
	}

	void CleanSimulationContext(TSharedPtr<Dataflow::FSimulationContext>& SimulationContext)
	{
		if(const TObjectPtr<AChaosCacheManager> CacheManager = Cast<AChaosCacheManager>(SimulationContext->GetRootActor()))
		{
			if(SimulationContext->GetCachingMode() == ECachingMode::RecordCache)
			{
				CacheManager->EndEvaluate();
			}

			//  Clear the observed components
			CacheManager->ClearObservedComponents();
			
			TInlineComponentArray<UPrimitiveComponent*> PrimComponents;
			CacheManager->GetComponents(PrimComponents);

			// Unregister all the components
			for(UPrimitiveComponent* PrimComponent : PrimComponents)
			{
				PrimComponent->SelectionOverrideDelegate.Unbind();
				PrimComponent->UnregisterComponent();
				PrimComponent->DestroyComponent();
			}
		}

		// Reset the simulation context
		SimulationContext.Reset();
	}

	void BuildSimulationContext(const TObjectPtr<UObject>& SimulationOwner, const TObjectPtr<UDataflow>& SimulationGraph, const TObjectPtr<AChaosCacheManager>& CacheManager,
		const ECachingMode CachingMode, TSharedPtr<Dataflow::FSimulationContext>& SimulationContext)
	{
		if(CacheManager)
		{
			// Create a simulation context that will hold all the simulation datas
			SimulationContext = MakeShared<Dataflow::FSimulationContext>(
						SimulationOwner, SimulationGraph,
					Dataflow::FTimestamp::Invalid, CacheManager, CachingMode);

			// Evaluate the setup stage of the simulation graph
			EvaluateSimulationGraph(SimulationGraph, SimulationContext, 
					0.0f, 0.0f , ESimulationFlags::SetupSimulation);

			// Init the cache manager
			CacheManager->SetObservedComponentProperties(CacheManager->CacheMode);

			if(SimulationContext->GetCachingMode() == ECachingMode::RecordCache)
			{
				CacheManager->BeginEvaluate();
			}
		}
	}
	
	void FDataflowSimulationTask::DoWork()
	{
		if(SimulationGraph)
		{
			const int32 NumFrames = (MaxTime-MinTime) / DeltaTime;
			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				if (!TaskManager->bCancelled.load())
				{
					// Compute the simulation time that will be sent to the graph
					const float SimulationTime = MinTime + (FrameIndex+1) * DeltaTime;

					// Evaluate the simulation graph update stage
					EvaluateSimulationGraph(SimulationGraph, SimulationContext,
						DeltaTime, SimulationTime, ESimulationFlags::UpdateSimulation);

					// Finish the frame
					TaskManager->SimulationResource->FinishFrame();
				}
				else
				{
					break;
				}
			}
		}
	}
	
	bool FDataflowTaskManager::AllocateSimulationResource(const TObjectPtr<UDataflow> SimulationGraph, const int32 SamplingRate, const TObjectPtr<UChaosCacheCollection>& CacheCollection)
	{
		SimulationWorld = UWorld::CreateWorld(EWorldType::Editor, false);
		TObjectPtr<AChaosCacheManager> CacheManager = SimulationWorld->SpawnActor<AChaosCacheManager>(AChaosCacheManager::StaticClass());

		CacheManager->StartMode = EStartMode::Timed;
		CacheManager->CacheMode = ECacheMode::Record;
		CacheManager->CacheCollection = CacheCollection;
		
		SimulationResource = MakeShared<FDataflowSimulationResource>();
		SimulationResource->NumSimulatedFrames = &NumSimulatedFrames;
		SimulationResource->bCancelled = &bCancelled;
		
		if(SimulationTask.IsValid())
		{
			// Clone the simulation graph to avoid evaluating the graph on another thread while modifying it on the game thread
			SimulationTask->GetTask().SimulationGraph = DuplicateObject<UDataflow>(SimulationGraph, SimulationWorld, NAME_None);

			// Build the simulation context
			BuildSimulationContext(SimulationTask->GetTask().SimulationGraph, SimulationTask->GetTask().SimulationGraph,
				CacheManager, ECachingMode::RecordCache, SimulationTask->GetTask().SimulationContext);

			// Compute the simulation duration in order to run the simulation
			const float Duration = GetCacheDuration(SimulationTask->GetTask().SimulationGraph, SimulationTask->GetTask().SimulationContext, SamplingRate,
				SimulationTask->GetTask().MinTime, SimulationTask->GetTask().MaxTime, NumFrames);

			// Update the delta time 
			SimulationTask->GetTask().DeltaTime = Duration / NumFrames;
		}

		return true;
	}
	
	void FDataflowTaskManager::FreeSimulationResource()
	{
		if (SimulationTask.IsValid())
		{
			SimulationTask->EnsureCompletion();

			CleanSimulationContext(SimulationTask->GetTask().SimulationContext);
		}
		
		SimulationResource.Reset();
		SimulationWorld->DestroyWorld(false);
	}

	void FDataflowTaskManager::CancelSimulationGeneration()
	{
		bCancelled.store(true);
		SimulationTask->TryAbandonTask();
	}
	
	FDataflowSimulationGenerator::FDataflowSimulationGenerator()
	{}

	FDataflowSimulationGenerator::~FDataflowSimulationGenerator()
	{
		if (TaskManager != nullptr)
		{
			TaskManager->FreeSimulationResource();
		}
	}

	void FDataflowSimulationGenerator::Tick(float DeltaTime)
	{
		if (PendingAction == EDataflowGeneratorActions::StartGenerate)
		{
			StartGenerateSimulation();
		}
		else if (PendingAction == EDataflowGeneratorActions::TickGenerate)
		{
			TickGenerateSimulation();
		}
	}

	TStatId FDataflowSimulationGenerator::GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDataflowSimulationGenerator, STATGROUP_Tickables);
	}

	void FDataflowSimulationGenerator::StartGenerateSimulation()
	{
		check(PendingAction == EDataflowGeneratorActions::StartGenerate);
		
		if (TaskManager != nullptr)
		{
			UE_LOG(LogDataflowSimulationGenerator, Error, TEXT("Previous generation is still running."));
			PendingAction = EDataflowGeneratorActions::NoAction;
			return;
		}
		if(!SimulationGraph)
		{
			UE_LOG(LogDataflowSimulationGenerator, Error, TEXT("No simulation graph available in the generator"));
			PendingAction = EDataflowGeneratorActions::NoAction;
			return;
		}
		TaskManager = TSharedPtr<FDataflowTaskManager>(new FDataflowTaskManager);

		TaskManager->SimulationTask = MakeUnique<FAsyncTask<FDataflowSimulationTask>>();
		TaskManager->SimulationTask->GetTask().TaskManager = TaskManager;
		
		TaskManager->AllocateSimulationResource(SimulationGraph, SamplingRate, CacheCollection);
		
		TaskManager->SimulationTask->StartBackgroundTask();
	
		FAsyncTaskNotificationConfig NotificationConfig;
		NotificationConfig.TitleText = LOCTEXT("SimulateDataflow", "Simulating Dataflow Content");
		NotificationConfig.ProgressText = FText::FromString(TEXT("0%"));
		NotificationConfig.bCanCancel = true;
		NotificationConfig.bKeepOpenOnSuccess = true;
		NotificationConfig.bKeepOpenOnFailure = true;
		TaskManager->AsyncNotification = MakeUnique<FAsyncTaskNotification>(NotificationConfig);
		TaskManager->StartTime = FDateTime::UtcNow();
		TaskManager->LastUpdateTime = TaskManager->StartTime;
	
		PendingAction = EDataflowGeneratorActions::TickGenerate;
	}
	
	void FDataflowSimulationGenerator::TickGenerateSimulation()
	{
		check(PendingAction == EDataflowGeneratorActions::TickGenerate && TaskManager != nullptr);
			
		bool bFinished = false;
		const bool bCancelled = TaskManager->AsyncNotification->GetPromptAction() == EAsyncTaskNotificationPromptAction::Cancel;
		if (TaskManager->SimulationTask->IsDone())
		{
			bFinished = true;
		}
		else if (bCancelled)
		{
			TaskManager->CancelSimulationGeneration();
			bFinished = true;
		}
			
		if (!bFinished)
		{
			const FDateTime CurrentTime = FDateTime::UtcNow();
			const double SinceLastUpdate = (CurrentTime - TaskManager->LastUpdateTime).GetTotalSeconds();
			if (SinceLastUpdate < 0.2)
			{
				return;
			}
			
			const int32 NumSimulatedFrames = TaskManager->NumSimulatedFrames.load();
			const int32 NumTotalFrames = TaskManager->NumFrames;
			const FText ProgressMessage = FText::FromString(FString::Printf(TEXT("Finished %d/%d, %.1f%%"), NumSimulatedFrames, NumTotalFrames, 100.0 * NumSimulatedFrames / NumTotalFrames));
			TaskManager->AsyncNotification->SetProgressText(ProgressMessage);
			TaskManager->LastUpdateTime = CurrentTime;
		}
		else
		{
			FreeTaskResource(bCancelled);
			PendingAction = EDataflowGeneratorActions::NoAction;
		}
	}

	void FDataflowSimulationGenerator::SetSimulationGraph(const TObjectPtr<UDataflow>& DataflowGraph) 
	{
		SimulationGraph = DataflowGraph;
	}

	void FDataflowSimulationGenerator::SetSamplingRate(const int32 FrameRate) 
	{
		SamplingRate = FrameRate;
	}
	
	void FDataflowSimulationGenerator::SetCacheCollection(const TObjectPtr<UChaosCacheCollection>& ChaosCache) 
	{
		CacheCollection = ChaosCache;
	}
	
	void FDataflowSimulationGenerator::RequestGeneratorAction(EDataflowGeneratorActions ActionType)
	{
		if (PendingAction != EDataflowGeneratorActions::NoAction)
		{
			return;
		}
		PendingAction = ActionType;
	}
	
	void FDataflowSimulationGenerator::FreeTaskResource(bool bCancelled)
	{
		TaskManager->AsyncNotification->SetProgressText(LOCTEXT("Finishing", "Finishing, please wait"));
		TaskManager->FreeSimulationResource();
		const FDateTime CurrentTime = FDateTime::UtcNow();
		UE_LOG(LogDataflowSimulationGenerator, Log, TEXT("Simulation finished in %f seconds"), (CurrentTime - TaskManager->StartTime).GetTotalSeconds());
	
		{
			// TODO : save the result to the chaos cache collection
		}
		if (bCancelled)
		{
			TaskManager->AsyncNotification->SetProgressText(LOCTEXT("Cancelled", "Cancelled"));
			TaskManager->AsyncNotification->SetComplete(false);
		}
		else
		{
			TaskManager->AsyncNotification->SetProgressText(LOCTEXT("Finished", "Finished"));
			TaskManager->AsyncNotification->SetComplete(true);
		}
		TaskManager.Reset();
	}
};

#undef LOCTEXT_NAMESPACE
