// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTCpuModule.h"
#include "NNE.h"
#include "NNERuntimeORTCpu.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakInterfacePtr.h"

void FNNERuntimeORTCpuModule::StartupModule()
{
	if (FModuleManager::Get().IsModuleLoaded("NNERuntimeORT"))
	{
		UE_LOG(LogNNE, Warning, TEXT("NNERuntimeORTCpu startup aborted, NNERuntimeORT plugin is active and provide the Cpu runtime, please deactivate NNERuntimeORTCpu plugin and make sure it has been removed from the .uplugin."));
		return;
	}

	// NNE runtime startup
	NNERuntimeORTCpu = NewObject<UNNERuntimeORTCustomCpuImpl>();
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());
		
		NNERuntimeORTCpu->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeCPUInterface);
	}
}

void FNNERuntimeORTCpuModule::ShutdownModule()
{
	// NNE runtime shutdown
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());
		
		UE::NNE::UnregisterRuntime(RuntimeCPUInterface);
		NNERuntimeORTCpu->RemoveFromRoot();
		NNERuntimeORTCpu.Reset();
	}
}

IMPLEMENT_MODULE(FNNERuntimeORTCpuModule, NNERuntimeORTCpu);
