// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTModule.h"
#include "NNE.h"
#include "NNERuntimeORT.h"
#include "NNEUtilitiesORTIncludeHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "UObject/WeakInterfacePtr.h"

namespace UE::NNERuntimeORT::Private::DllHelper
{
	bool GetDllHandle(const FString& DllPath, TArray<void*>& DllHandles)
	{
		void *DllHandle = nullptr;

		if (!FPaths::FileExists(DllPath))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to find the third party library %s."), *DllPath);
			return false;
		}
		
		DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

		if (!DllHandle)
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to load the third party library %s."), *DllPath);
			return false;
		}

		DllHandles.Add(DllHandle);
		return true;
	}
	}

void FNNERuntimeORTModule::StartupModule()
{
	const FString PluginDir = IPluginManager::Get().FindPlugin("NNERuntimeORT")->GetBaseDir();
	const FString OrtSharedLibPath = FPaths::Combine(PluginDir, TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIME_SHAREDLIB_PATH)));

	if (!UE::NNERuntimeORT::Private::DllHelper::GetDllHandle(OrtSharedLibPath, DllHandles))
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to load OnnxRuntime shared library. ORT Runtimes won't be available."));
		return;
	}

	Ort::InitApi();

#if PLATFORM_WINDOWS
	// NNE runtime ORT Dml startup
	NNERuntimeORTDml = NewObject<UNNERuntimeORTDml>();
	if (NNERuntimeORTDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

		NNERuntimeORTDml->Init();
		NNERuntimeORTDml->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeDmlInterface);
	}
#endif

	// NNE runtime ORT Cpu startup
	NNERuntimeORTCpu = NewObject<UNNERuntimeORTCpu>();
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());

		NNERuntimeORTCpu->Init();
		NNERuntimeORTCpu->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeCPUInterface);
	}
}

void FNNERuntimeORTModule::ShutdownModule()
{
	// NNE runtime ORT Cpu shutdown
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());

		UE::NNE::UnregisterRuntime(RuntimeCPUInterface);
		NNERuntimeORTCpu->RemoveFromRoot();
		NNERuntimeORTCpu.Reset();
	}

	// NNE runtime ORT Dml shutdown
	if (NNERuntimeORTDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

		UE::NNE::UnregisterRuntime(RuntimeDmlInterface);
		NNERuntimeORTDml->RemoveFromRoot();
		NNERuntimeORTDml.Reset();
	}

	// Free the dll handles
	for(void* DllHandle : DllHandles)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
	}
	DllHandles.Empty();
}

IMPLEMENT_MODULE(FNNERuntimeORTModule, NNERuntimeORT);