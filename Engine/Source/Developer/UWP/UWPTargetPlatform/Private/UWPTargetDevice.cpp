// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UWPTargetDevice.h"

#include "Misc/Paths.h"

#if PLATFORM_WINDOWS || PLATFORM_UWP
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "Windows/ComPointer.h"
#include <AppxPackaging.h>
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Misc/Base64.h"
#include "Containers/UnrealString.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ScopeLock.h"

#pragma warning(push)
#pragma warning(disable:4265 4459)

#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#pragma warning(pop)

#include <shlwapi.h>

#if (NTDDI_VERSION < NTDDI_WIN8)
#pragma push_macro("NTDDI_VERSION")
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8
//this header cannot be directly imported because of current _WIN32_WINNT less then 0x0602 (the value assigned in UEBuildWindows.cs:139)
//the macro code added from couple interface declarations, it doesn't affect to any imported function
#include <shobjidl.h> 
#pragma pop_macro("NTDDI_VERSION")
#else
#include <shobjidl.h> 
#endif
namespace WindowsMixedReality
{
	bool PackageProject(const wchar_t* StreamPath, bool PathIsActuallyPackage, const wchar_t* Params, UINT*& OutProcessId)
	{

		using namespace Microsoft::WRL;
		ComPtr<IAppxFactory> AppxFactory;
		if (FAILED(CoCreateInstance(CLSID_AppxFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&AppxFactory))))
		{
			return false;
		}

		ComPtr<IStream> ReaderStream;
		if (FAILED(SHCreateStreamOnFileEx(StreamPath, STGM_READ, 0, FALSE, nullptr, &ReaderStream)))
		{
			return false;
		}

		ComPtr<IAppxManifestReader> ManifestReader;
		if (PathIsActuallyPackage)
		{
			ComPtr<IAppxPackageReader> PackageReader;
			if (FAILED(AppxFactory->CreatePackageReader(ReaderStream.Get(), &PackageReader)))
			{
				return false;
			}

			if (FAILED(PackageReader->GetManifest(&ManifestReader)))
			{
				return false;
			}
		}
		else
		{
			if (FAILED(AppxFactory->CreateManifestReader(ReaderStream.Get(), &ManifestReader)))
			{
				return false;
			}
		}

		ComPtr<IAppxManifestApplicationsEnumerator> AppEnumerator;
		if (FAILED(ManifestReader->GetApplications(&AppEnumerator)))
		{
			return false;
		}

		ComPtr<IAppxManifestApplication> ApplicationMetadata;
		if (FAILED(AppEnumerator->GetCurrent(&ApplicationMetadata)))
		{
			return false;
		}

		ComPtr<IPackageDebugSettings> PackageDebugSettings;
		if (SUCCEEDED(CoCreateInstance(CLSID_PackageDebugSettings, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&PackageDebugSettings))))
		{
			ComPtr<IAppxManifestPackageId> ManifestPackageId;
			if (SUCCEEDED(ManifestReader->GetPackageId(&ManifestPackageId)))
			{
				LPWSTR PackageFullName = nullptr;
				// Avoid warning C6387 (static analysis assumes PackageFullName could be null even if the GetPackageFullName call succeeds.
				if (SUCCEEDED(ManifestPackageId->GetPackageFullName(&PackageFullName)) && PackageFullName != nullptr)
				{
					PackageDebugSettings->EnableDebugging(PackageFullName, nullptr, nullptr);
					CoTaskMemFree(PackageFullName);
				}
			}
		}

		LPWSTR Aumid = nullptr;
		// Avoid warning C6387 (static analysis assumes Aumid could be null even if the GetAppUserModelId call succeeds.
		if (FAILED(ApplicationMetadata->GetAppUserModelId(&Aumid)) || Aumid == nullptr)
		{
			return false;
		}

		bool ActivationSuccess = false;
		ComPtr<IApplicationActivationManager> ActivationManager;
		if (SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ActivationManager))))
		{
			DWORD NativeProcessId;
			if (SUCCEEDED(ActivationManager->ActivateApplication(Aumid, Params, AO_NONE, &NativeProcessId)))
			{
				if (OutProcessId != nullptr)
				{
					*OutProcessId = NativeProcessId;
				}
				ActivationSuccess = true;
			}
		}

		CoTaskMemFree(Aumid);
		return ActivationSuccess;

	}
}
bool FUWPTargetDevice::Deploy(const FString& SourceFolder, FString& OutAppId)
{
	return true;
}

bool FUWPTargetDevice::Launch(const FString& AppId, EBuildConfigurations::Type BuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId)
{
	return false;
}

bool FUWPTargetDevice::Run(const FString& ExecutablePath, const FString& Params, uint32* OutProcessId)
{
	CoInitialize(nullptr);

	// Currently even packaged builds get an exe name in here which kind of works because we 
	// don't yet support remote deployment and so the loose structure the package was created 
	// from is probably in place on this machine.  So the code will read the manifest from the
	// loose version, but actually launch the package (since that's what's registered).
	bool PathIsActuallyPackage = FPaths::GetExtension(ExecutablePath) == TEXT("appx");
	FString StreamPath;
	if (PathIsActuallyPackage)
	{
		StreamPath = ExecutablePath;
	}
	else
	{
		StreamPath = FPaths::Combine(*FPaths::GetPath(ExecutablePath), TEXT("../../.."));
		StreamPath = FPaths::Combine(*StreamPath, TEXT("AppxManifest.xml"));
	}

	return WindowsMixedReality::PackageProject(*StreamPath, PathIsActuallyPackage, *Params, OutProcessId);
}

#include "Windows/HideWindowsPlatformTypes.h"
