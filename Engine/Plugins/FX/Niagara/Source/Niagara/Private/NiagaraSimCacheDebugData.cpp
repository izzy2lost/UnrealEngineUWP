// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCacheDebugData.h"
#include "NiagaraSimCacheHelper.h"

UNiagaraSimCacheDebugData::UNiagaraSimCacheDebugData(const FObjectInitializer& ObjectInitializer)
{
}

void UNiagaraSimCacheDebugData::CaptureFrame(FNiagaraSimCacheHelper& Helper, int FrameNumber)
{
   	Frames.SetNum(FrameNumber + 1);
	if (const FNiagaraParameterStore* ParameterStore = Helper.SystemInstance->GetOverrideParameters())
	{
		Frames[FrameNumber].OverrideParameters = *ParameterStore;
	}
}
