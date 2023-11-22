// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeTaskPipeline.h"

#include "AssetCompilingManager.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "InterchangeEngineLogPrivate.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "Materials/MaterialInterface.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeFactoryBaseNode.h"
#include "Stats/Stats.h"
#include "Templates/SharedPointer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/WeakObjectPtrTemplates.h"



void UE::Interchange::FTaskPipeline::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskPipeline::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PipelinePreImport)
#endif

	TOptional<FGCScopeGuard> GCScopeGuard;
	if (!IsInGameThread())
	{
		GCScopeGuard.Emplace();
	}

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	check(AsyncHelper.IsValid());

	if (UInterchangePipelineBase* Pipeline = PipelineBase.Get())
	{
		Pipeline->SetResultsContainer(AsyncHelper->AssetImportResult->GetResults());

		for (int32 GraphIndex = 0; GraphIndex < AsyncHelper->BaseNodeContainers.Num(); ++GraphIndex)
		{
			//Verify if the task was cancel
			if (AsyncHelper->bCancel)
			{
				return;
			}

			if (ensure(AsyncHelper->BaseNodeContainers[GraphIndex].IsValid()))
			{
				Pipeline->ScriptedExecutePipeline(AsyncHelper->BaseNodeContainers[GraphIndex].Get(), AsyncHelper->SourceDatas, AsyncHelper->ContentBasePath);
			}
		}
	}
}

void UE::Interchange::FTaskWaitAssetCompilation::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskWaitAssetCompilation::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
		INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(WaitAssetCompilation)
#endif

#if WITH_EDITOR

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	if (!ensure(AsyncHelper.IsValid()) || AsyncHelper->bCancel)
	{
		return;
	}

	if (!ensure(AsyncHelper->SourceDatas.IsValidIndex(SourceIndex)))
	{
		return;
	}

	TArray<UObject*> ImportedObjects;

	auto FillImportedObjectsFromSource = [&ImportedObjects](const TArray<UE::Interchange::FImportAsyncHelper::FImportedObjectInfo>& ImportedInfos)
		{
			ImportedObjects.Reserve(ImportedObjects.Num() + ImportedInfos.Num());
			for (const UE::Interchange::FImportAsyncHelper::FImportedObjectInfo& ImportedInfo : ImportedInfos)
			{
				ImportedObjects.Add(ImportedInfo.ImportedObject);
			}
		};

	AsyncHelper->IterateImportedAssets(SourceIndex, FillImportedObjectsFromSource);
	AsyncHelper->IterateImportedSceneObjects(SourceIndex, FillImportedObjectsFromSource);

	//Make sure all assets compilation are done before calling the pipeline post import task, let other thread execute if assets are not compile yet and wait 50ms before a new query
	FPlatformProcess::ConditionalSleep([&ImportedObjects]()
		{
			//Compilation status cannot be ask in async thread, query the compile status on the main thread with a small fast function
			//This ensure we dont stall the main thread until all assets are compile.
			bool bCompilationFinish = false;
			Async(EAsyncExecution::TaskGraphMainThread, [&bCompilationFinish, &ImportedObjects]()
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskWaitAssetCompilation::DoTask::IsCompilingLambda_GameThread);
					//Make sure all asset compiling managers are up to date, In case the game thread is waiting for the import to finish (like automation test or synchronous import)
					FAssetCompilingManager::Get().ProcessAsyncTasks();

					bCompilationFinish = true;
					for (int32 ObjectIndex = 0; ObjectIndex < ImportedObjects.Num(); ++ObjectIndex)
					{
						UObject* ImportObject = ImportedObjects[ObjectIndex];
						if (UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(ImportObject))
						{
							if (MaterialInterface->IsCompiling())
							{
								bCompilationFinish = false;
								break;
							}
						}
						if (IInterface_AsyncCompilation* AssetCompilationInterface = Cast<IInterface_AsyncCompilation>(ImportObject))
						{
							if (AssetCompilationInterface->IsCompiling())
							{
								bCompilationFinish = false;
								break;
							}
						}
					}
				}).Wait();
			return bCompilationFinish;
		}, 0.05f);

#endif //WITH_EDITOR
}

void UE::Interchange::FTaskPipelinePostImport::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UE::Interchange::FTaskPipelinePostImport::DoTask)
#if INTERCHANGE_TRACE_ASYNCHRONOUS_TASK_ENABLED
	INTERCHANGE_TRACE_ASYNCHRONOUS_TASK(PipelinePostImport)
#endif

	TOptional<FGCScopeGuard> GCScopeGuard;
	if (!IsInGameThread())
	{
		GCScopeGuard.Emplace();
	}

	TSharedPtr<FImportAsyncHelper, ESPMode::ThreadSafe> AsyncHelper = WeakAsyncHelper.Pin();
	if (!ensure(AsyncHelper.IsValid()) || AsyncHelper->bCancel)
	{
		return;
	}

	if (!ensure(AsyncHelper->Pipelines.IsValidIndex(PipelineIndex)) || !ensure(AsyncHelper->BaseNodeContainers.IsValidIndex(SourceIndex)))
	{
		return;
	}
	UInterchangePipelineBase* PipelineBase = AsyncHelper->Pipelines[PipelineIndex];
	TArray<FString> NodeUniqueIDs;
	TArray<UObject*> ImportedObjects;
	TArray<bool> IsAssetsReimported;

	auto FillImportedObjectsFromSource =
		[&NodeUniqueIDs, &ImportedObjects, &IsAssetsReimported, this](const TArray<UE::Interchange::FImportAsyncHelper::FImportedObjectInfo>& ImportedInfos)
		{
			NodeUniqueIDs.Reserve(NodeUniqueIDs.Num() + ImportedInfos.Num());
			ImportedObjects.Reserve(ImportedObjects.Num() + ImportedInfos.Num());
			IsAssetsReimported.Reserve(IsAssetsReimported.Num() + ImportedInfos.Num());
			for (const UE::Interchange::FImportAsyncHelper::FImportedObjectInfo& ImportedInfo : ImportedInfos)
			{
				NodeUniqueIDs.Add(ImportedInfo.FactoryNode->GetUniqueID());
				ImportedObjects.Add(ImportedInfo.ImportedObject);
				IsAssetsReimported.Add(ImportedInfo.bIsReimport);
			}
		};

	AsyncHelper->IterateImportedAssets(SourceIndex, FillImportedObjectsFromSource);
	AsyncHelper->IterateImportedSceneObjects(SourceIndex, FillImportedObjectsFromSource);

	if (!ensure(NodeUniqueIDs.Num() == ImportedObjects.Num()))
	{
		//We do not execute the script if we cannot give proper parameter
		return;
	}

	//Get the Container from the async helper
	UInterchangeBaseNodeContainer* NodeContainer = AsyncHelper->BaseNodeContainers[SourceIndex].Get();
	if (!ensure(NodeContainer))
	{
		return;
	}
	UInterchangePipelineBase* Pipeline = AsyncHelper->Pipelines[PipelineIndex];

	//Call the pipeline for each asset created by this import
	for (int32 ObjectIndex = 0; ObjectIndex < ImportedObjects.Num(); ++ObjectIndex)
	{
		Pipeline->ScriptedExecutePostImportPipeline(NodeContainer, NodeUniqueIDs[ObjectIndex], ImportedObjects[ObjectIndex], IsAssetsReimported[ObjectIndex]);
	}
}
