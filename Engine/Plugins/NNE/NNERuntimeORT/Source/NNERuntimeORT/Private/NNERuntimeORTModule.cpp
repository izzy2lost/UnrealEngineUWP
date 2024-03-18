// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeORTModule.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "NNE.h"
#include "NNEOnnxruntime.h"
#include "NNERuntimeORT.h"
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

} // namespace UE::NNERuntimeRDG::Private::Dml

void FNNERuntimeORTModule::StartupModule()
{
	const FString PluginDir = IPluginManager::Get().FindPlugin("NNERuntimeORT")->GetBaseDir();
	const FString OrtSharedLibPath = FPaths::Combine(PluginDir, TEXT(PREPROCESSOR_TO_STRING(ONNXRUNTIME_SHAREDLIB_PATH)));

	if (!UE::NNERuntimeORT::Private::DllHelper::GetDllHandle(OrtSharedLibPath, DllHandles))
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to load ONNX Runtime shared library. ORT Runtimes won't be available."));
		return;
	}

	void* OrtDllHandle = DllHandles.Last();

#if PLATFORM_WINDOWS
	const FString ModuleDir = FPlatformProcess::GetModulesDirectory();
	const FString DirectMLSharedLibPath = FPaths::Combine(ModuleDir, TEXT(PREPROCESSOR_TO_STRING(DIRECTML_PATH)), TEXT("DirectML.dll"));

	const bool bDirectMLDllLoaded = UE::NNERuntimeORT::Private::DllHelper::GetDllHandle(DirectMLSharedLibPath, DllHandles);
	if (!bDirectMLDllLoaded)
	{
		UE_LOG(LogNNE, Error, TEXT("Failed to load DirectML shared library. ORT Dml Runtime won't be available."));
	}
#endif // PLATFORM_WINDOWS

	TUniquePtr<UE::NNEOnnxruntime::OrtApiFunctions> OrtApiFunctions = UE::NNEOnnxruntime::LoadApiFunctions(OrtDllHandle);
	if (!OrtApiFunctions.IsValid())
	{
		UE_LOG(LogNNE, Fatal, TEXT("Failed to load ONNX Runtime shared library functions!"));
		return;
	}

	Ort::InitApi(OrtApiFunctions->OrtGetApiBase()->GetApi(ORT_API_VERSION));

#if PLATFORM_WINDOWS
	if (bDirectMLDllLoaded)
	{
#if WITH_EDITOR
		// NNE runtime ORT Dml startup
		NNERuntimeORTDmlEditor = NewObject<UNNERuntimeORTDmlEditor>();
		if (NNERuntimeORTDmlEditor.IsValid())
		{
			TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDmlEditor.Get());

			NNERuntimeORTDmlEditor->Init();
			NNERuntimeORTDmlEditor->AddToRoot();
			UE::NNE::RegisterRuntime(RuntimeDmlInterface);
		}
#else
		// NNE runtime ORT Dml startup
		NNERuntimeORTDml = NewObject<UNNERuntimeORTDml>();
		if (NNERuntimeORTDml.IsValid())
		{
			TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

			NNERuntimeORTDml->Init();
			NNERuntimeORTDml->AddToRoot();
			UE::NNE::RegisterRuntime(RuntimeDmlInterface);
		}
#endif // WITH_EDITOR
	}
#endif // PLATFORM_WINDOWS

#if WITH_EDITOR
	// NNE runtime ORT Cpu startup
	NNERuntimeORTCpu = NewObject<UNNERuntimeORTCpu>();
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());

		NNERuntimeORTCpu->Init();
		NNERuntimeORTCpu->AddToRoot();
		UE::NNE::RegisterRuntime(RuntimeCPUInterface);
	}
#endif // WITH_EDITOR
}

void FNNERuntimeORTModule::ShutdownModule()
{
#if WITH_EDITOR
	// NNE runtime ORT Cpu shutdown
	if (NNERuntimeORTCpu.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeCPUInterface(NNERuntimeORTCpu.Get());

		UE::NNE::UnregisterRuntime(RuntimeCPUInterface);
		NNERuntimeORTCpu->RemoveFromRoot();
		NNERuntimeORTCpu.Reset();
	}

	// NNE runtime ORT Dml shutdown
	if (NNERuntimeORTDmlEditor.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDmlEditor.Get());

		UE::NNE::UnregisterRuntime(RuntimeDmlInterface);
		NNERuntimeORTDmlEditor->RemoveFromRoot();
		NNERuntimeORTDmlEditor.Reset();
	}
#else
	// NNE runtime ORT Dml shutdown
	if (NNERuntimeORTDml.IsValid())
	{
		TWeakInterfacePtr<INNERuntime> RuntimeDmlInterface(NNERuntimeORTDml.Get());

		UE::NNE::UnregisterRuntime(RuntimeDmlInterface);
		NNERuntimeORTDml->RemoveFromRoot();
		NNERuntimeORTDml.Reset();
	}
#endif // WITH_EDITOR

	// Free the dll handles
	for(void* DllHandle : DllHandles)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
	}
	DllHandles.Empty();
}

IMPLEMENT_MODULE(FNNERuntimeORTModule, NNERuntimeORT);