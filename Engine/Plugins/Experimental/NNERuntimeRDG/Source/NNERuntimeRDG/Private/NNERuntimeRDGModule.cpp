// Copyright Epic Games, Inc. All Rights Reserved.

#include "HAL/IConsoleManager.h"
#include "NNERuntimeRDGModule.h"
#include "NNE.h"
#include "NNERuntimeRDGHlsl.h"
#include "UObject/WeakInterfacePtr.h"

static TAutoConsoleVariable<int32> CVarHlslModelOptimization(
	TEXT("nne.hlsl.ModelOptimization"),
	1,
	TEXT("Allows model optimizations when model are cooked for the HLSL runtime.\n")
	TEXT(" 0: disabled\n")
	TEXT(" 1: enabled (default)")
#if !WITH_EDITOR
	,ECVF_ReadOnly
#endif
	);

void FNNERuntimeRDGModule::StartupModule()
{
	// NNE runtime ORT Cpu startup
	NNERuntimeRDGHlsl = NewObject<UNNERuntimeRDGHlslImpl>();
	if (NNERuntimeRDGHlsl.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeRDGHlsl.Get());

		NNERuntimeRDGHlsl->Init();
		NNERuntimeRDGHlsl->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeCPUInterface);
	}
}

void FNNERuntimeRDGModule::ShutdownModule()
{
	// NNE runtime ORT Cpu shutdown
	if (NNERuntimeRDGHlsl.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeRDGHlsl.Get());

		UE::NNE::UnregisterRuntime(RuntimeCPUInterface);
		NNERuntimeRDGHlsl->RemoveFromRoot();
		NNERuntimeRDGHlsl.Reset();
	}
}

IMPLEMENT_MODULE(FNNERuntimeRDGModule, NNERuntimeRDG);
