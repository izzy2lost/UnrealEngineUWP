// Copyright Epic Games, Inc. All Rights Reserved.

#include "ODSC/ODSCManager.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "ODSCLog.h"
#include "ODSCThread.h"
#include "Containers/BackgroundableTicker.h"
#include "Materials/MaterialInstance.h"

DEFINE_LOG_CATEGORY(LogODSC);

// FODSCManager

FODSCManager* GODSCManager = nullptr;

FODSCManager::FODSCManager()
	: FTSTickerObjectBase(0.0f, FTSBackgroundableTicker::GetCoreTicker())
{
	FString Host;
	const bool bODSCEnabled = FParse::Value(FCommandLine::Get(), TEXT("-odschost="), Host);

	if (IsRunningCookOnTheFly() || bODSCEnabled)
	{
		FCoreDelegates::OnEnginePreExit.AddRaw(this, &FODSCManager::OnEnginePreExit);
		Thread = new FODSCThread(Host);
		Thread->StartThread();
		OnScreenMessagesHandle = FCoreDelegates::OnGetOnScreenMessages.AddLambda([this](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
			{
				if (Thread && Thread->HasPendingRequests())
				{
					OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::FromString(FString::Printf(TEXT("Recompiling shaders"))));
				}
			}
		);
	}
}

FODSCManager::~FODSCManager()
{
	if (OnScreenMessagesHandle.IsValid())
	{
		FCoreDelegates::OnGetOnScreenMessages.Remove(OnScreenMessagesHandle);
	}

	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	StopThread();
}

void FODSCManager::OnEnginePreExit()
{
	StopThread();
}

void FODSCManager::StopThread()
{
	if (Thread)
	{
		Thread->StopThread();
		delete Thread;
		Thread = nullptr;
	}
}

bool FODSCManager::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FODSCManager_Tick);

	if (IsHandlingRequests())
	{
		Thread->Wakeup();

		TArray<FODSCMessageHandler*> CompletedThreadedRequests;
		Thread->GetCompletedRequests(CompletedThreadedRequests);

		bool bFlushAsyncLoading = HasAsyncLoadingInstances();

		if (CompletedThreadedRequests.Num() && bFlushAsyncLoading)
		{
			FlushAsyncLoading();
		}

		// Finish and remove any completed requests
		for (FODSCMessageHandler* CompletedRequest : CompletedThreadedRequests)
		{
			check(CompletedRequest);
			ProcessCookOnTheFlyShaders(false, CompletedRequest->GetMeshMaterialMaps(), CompletedRequest->GetMaterialsToLoad(), CompletedRequest->GetGlobalShaderMap());
			delete CompletedRequest;
		}
		// keep ticking
		return true;
	}
	// stop ticking
	return false;
}

void FODSCManager::AddThreadedRequest(
	const TArray<FString>& MaterialsToCompile,
	const FString& ShaderTypesToLoad,
	EShaderPlatform ShaderPlatform,
	ERHIFeatureLevel::Type FeatureLevel,
	EMaterialQualityLevel::Type QualityLevel,
	ODSCRecompileCommand RecompileCommandType
)
{
	if (IsHandlingRequests())
	{
		Thread->AddRequest(MaterialsToCompile, ShaderTypesToLoad, ShaderPlatform, FeatureLevel, QualityLevel, RecompileCommandType);
	}
}

void FODSCManager::AddThreadedShaderPipelineRequest(
	EShaderPlatform ShaderPlatform,
	ERHIFeatureLevel::Type FeatureLevel,
	EMaterialQualityLevel::Type QualityLevel,
	const FString& MaterialName,
	const FString& VertexFactoryName,
	const FString& PipelineName,
	const TArray<FString>& ShaderTypeNames,
	int32 PermutationId
)
{
	if (IsHandlingRequests())
	{
		Thread->AddShaderPipelineRequest(ShaderPlatform, FeatureLevel, QualityLevel, MaterialName, VertexFactoryName, PipelineName, ShaderTypeNames, PermutationId);
	}
}

static inline bool IsODSCActive()
{
	return GODSCManager && GODSCManager->IsHandlingRequests();
}

void FODSCManager::RegisterMaterialInstance(const UMaterialInstance* MaterialInstance)
{
	if (IsODSCActive() && MaterialInstance->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
	{
		FScopeLock Lock(&GODSCManager->MaterialInstancesCachedUniformExpressionsCS);
		TWeakObjectPtr<const UMaterialInstance>& MaterialInstanceSoftPtr = GODSCManager->MaterialInstancesCachedUniformExpressions.FindOrAdd(MaterialInstance);
		MaterialInstanceSoftPtr = TWeakObjectPtr<const UMaterialInstance>(MaterialInstance);
	}
} 

void FODSCManager::UnregisterMaterialInstance(const UMaterialInstance* MaterialInstance)
{
	if (GODSCManager != nullptr)
	{
		FScopeLock Lock(&GODSCManager->MaterialInstancesCachedUniformExpressionsCS);
		GODSCManager->MaterialInstancesCachedUniformExpressions.Remove(MaterialInstance);
	}
}

bool FODSCManager::HasAsyncLoadingInstances()
{
	FScopeLock Lock(&MaterialInstancesCachedUniformExpressionsCS);

	bool bHasAsyncLoadingInstances = false;
	for (auto Iter = MaterialInstancesCachedUniformExpressions.CreateIterator(); Iter; ++Iter)
	{
		const UMaterialInstance* MI = Iter.Value().Get();

		if (MI == nullptr || !MI->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
		{
			Iter.RemoveCurrent();
			continue;
		}

		bHasAsyncLoadingInstances = true;
	}

	return bHasAsyncLoadingInstances;
}
