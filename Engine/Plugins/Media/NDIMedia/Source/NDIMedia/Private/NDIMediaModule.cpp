// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDIMediaModule.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NDIMediaAPI.h"
#include "NDIMediaCapture.h"

#define LOCTEXT_NAMESPACE "FNDIMediaModule"

const NDIlib_v5* FNDIMediaModule::NDILib = nullptr;

void FNDIMediaModule::StartupModule()
{
	// Doubly Ensure that this handle is nullptr
	NDILibHandle = nullptr;
	NDILib = nullptr;

	if (!LoadModuleDependencies())
	{
		// Write an error message to the log.
		UE_LOG(LogNDIMedia, Error, TEXT("Unable to load \"Processing.NDI.Lib.x64.dll\" from the NDI 5 Runtime Directory."));

#if UE_EDITOR
		const FText& WarningMessage =
			LOCTEXT("NDIRuntimeMissing",
				"Cannot find \"Processing.NDI.Lib.x64.dll\" from the NDI 5 Runtime Directory. "
				"Continued usage of the plugin can cause instability within the editor.\r\n\r\n"

				"Please refer to the 'NDI IO Plugin for Unreal Engine Quickstart Guide' "
				"for additional information related to installation instructions for this plugin.\r\n\r\n");

		// Open a message box, showing that things will not work since the NDI Runtime Directory cannot be found
		if (FMessageDialog::Open(EAppMsgType::OkCancel, EAppReturnType::Ok, WarningMessage) == EAppReturnType::Ok)
		{
			FString URLResult = FString("");
			FPlatformProcess::LaunchURL(*FString(NDILIB_REDIST_URL), nullptr, &URLResult);
		}
#endif
	}
}

void FNDIMediaModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (NDILib != nullptr)
	{
		// Not required, but nice.
		NDILib->destroy();
		NDILib = nullptr;
	}

	// Free the dll handle
	if (NDILibHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(NDILibHandle);
		NDILibHandle = nullptr;
	}
}

bool FNDIMediaModule::LoadModuleDependencies()
{
	// Get the Binaries File Location
	FString DllPath = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("NDIMedia"))->GetBaseDir(), TEXT("/Binaries/ThirdParty/Win64"));
	FPlatformProcess::PushDllDirectory(*DllPath);
	DllPath = FPaths::Combine(DllPath, TEXT("Processing.NDI.Lib.x64.dll"));

	// We can't validate if it's valid, but we can determine if it's explicitly not.
	if (DllPath.Len() > 0)
	{
		// Load the DLL
		NDILibHandle = FPlatformProcess::GetDllHandle(*DllPath);

		// The main NDI entry point for dynamic loading if we got the library
		if (NDILibHandle)
		{
			typedef const NDIlib_v5* (*NDIlib_v5_load_ptr)(void);
			NDIlib_v5_load_ptr _NDILib_v5_load = static_cast<NDIlib_v5_load_ptr>(FPlatformProcess::GetDllExport(NDILibHandle, TEXT("NDIlib_v5_load")));
			if (_NDILib_v5_load)
			{
				NDILib = _NDILib_v5_load();
				if (NDILib != nullptr)
				{
					// Not required, but "correct" (see the SDK documentation)
					if (!NDILib->initialize())
					{
						NDILib = nullptr;

						// Write an error message to the log.
						UE_LOG(LogNDIMedia, Error, TEXT("Unable to initialize NDI library."));
					}
				}
			}
		}

		if (NDILib == nullptr)
		{
			if (NDILibHandle != nullptr)
			{
				// We were unable to initialize the library, so lets free the handle
				FPlatformProcess::FreeDllHandle(NDILibHandle);
				NDILibHandle = nullptr;
			}
		}
	}

	// Did we successfully load the NDI library?
	return NDILib != nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNDIMediaModule, NDIMedia)
