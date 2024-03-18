// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGModule.h"
#include "NNE.h"
#include "NNERuntimeRDGHlsl.h"
#include "UObject/WeakInterfacePtr.h"


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
