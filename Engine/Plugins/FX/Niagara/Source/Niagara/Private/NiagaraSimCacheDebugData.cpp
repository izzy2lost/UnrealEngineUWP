// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCacheDebugData.h"
#include "NiagaraSimCacheHelper.h"
#include "NiagaraEmitterInstanceImpl.h"

namespace NSCDebugDataPrivate
{
	template<typename TStringType>
	void AddParameterStore(FNiagaraSimCacheDebugDataFrame& FrameData, TStringType StoreName, const FNiagaraParameterStore& ParameterStore)
	{
		if (ParameterStore.Num() > 0)
		{
			FrameData.DebugParameterStores.FindOrAdd(StoreName) = ParameterStore;
		}
	}

	template<typename TStringType>
	void AddParameterStore(FNiagaraSimCacheDebugDataFrame& FrameData, TStringType StoreName, const FNiagaraScriptExecutionContextBase* ExecContext)
	{
		if (ExecContext && ExecContext->Parameters.ReadParameterVariables().Num() > 0)
		{
			FNiagaraParameterStore& DestStore = FrameData.DebugParameterStores.FindOrAdd(StoreName);
			ExecContext->Parameters.CopyParametersTo(DestStore, false, FNiagaraParameterStore::EDataInterfaceCopyMethod::Value);
		}
	}

	template<typename TStringType>
	void AddParameterStore(FNiagaraSimCacheDebugDataFrame& FrameData, TStringType StoreName, const FNiagaraComputeExecutionContext* ExecContext)
	{
		if (ExecContext)
		{
			AddParameterStore(FrameData, StoreName, ExecContext->CombinedParamStore);
		}
	}
}

UNiagaraSimCacheDebugData::UNiagaraSimCacheDebugData(const FObjectInitializer& ObjectInitializer)
{
}

void UNiagaraSimCacheDebugData::CaptureFrame(FNiagaraSimCacheHelper& Helper, int FrameNumber)
{
	using namespace NSCDebugDataPrivate;

   	Frames.SetNum(FrameNumber + 1);

	FNiagaraSimCacheDebugDataFrame& FrameData = Frames[FrameNumber];

	// Add Override Parameters
	if (const FNiagaraParameterStore* OverrideParameterStore = Helper.SystemInstance->GetOverrideParameters())
	{
		AddParameterStore(FrameData, TEXT("OverrideParameters"), *OverrideParameterStore);
	}

	// Add Instance Parameters
	AddParameterStore(FrameData, TEXT("InstanceParameters"), Helper.SystemInstance->GetInstanceParameters());

	// Add System Script Parameters
	if (FNiagaraSystemSimulation* SystemSimulation = Helper.SystemInstance->GetSystemSimulation().Get())
	{
		AddParameterStore(FrameData, TEXT("System Spawn"), SystemSimulation->GetSpawnExecutionContext());
		AddParameterStore(FrameData, TEXT("System Update"), SystemSimulation->GetUpdateExecutionContext());
	}

	// Add Emitter Parameters
	for (const FNiagaraEmitterInstanceRef& EmitterRef : Helper.SystemInstance->GetEmitters())
	{
		const FNiagaraEmitterHandle& EmitterHandle = EmitterRef->GetEmitterHandle();
		const FString EmitterName = EmitterHandle.GetName().ToString();

		AddParameterStore(FrameData, FString::Printf(TEXT("%s RendererBindings"), *EmitterName), EmitterRef->GetRendererBoundVariables());
		AddParameterStore(FrameData, FString::Printf(TEXT("%s GPUContext"), *EmitterName), EmitterRef->GetGPUContext());

		if (FNiagaraEmitterInstanceImpl* StatefulEmitter = EmitterRef->AsStateful())
		{
			AddParameterStore(FrameData, FString::Printf(TEXT("%s Spawn"), *EmitterName), &StatefulEmitter->GetSpawnExecutionContext());
			AddParameterStore(FrameData, FString::Printf(TEXT("%s Update"), *EmitterName), &StatefulEmitter->GetUpdateExecutionContext());
		}
	}
}
