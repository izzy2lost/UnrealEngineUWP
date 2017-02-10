// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UWPTargetPlatformPrivatePCH.h"

#include "UWPTargetDevice.h"

#include "AllowWindowsPlatformTypes.h"

//#include <wrl/client.h>
//#include <wrl/wrappers/corewrappers.h>

#include <shlwapi.h>
#include <shobjidl.h>
#include <AppxPackaging.h>
#include "Windows/ComPointer.h"
//#include "ComPointer.h"
//#include <Windows.Management.Deployment.h>

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

	TComPtr<IAppxFactory> AppxFactory;
	if (FAILED(CoCreateInstance(CLSID_AppxFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&AppxFactory))))
	{
		return false;
	}

	TComPtr<IStream> ReaderStream;
	if (FAILED(SHCreateStreamOnFileEx(*StreamPath, STGM_READ, 0, FALSE, nullptr, &ReaderStream)))
	{
		return false;
	}

	TComPtr<IAppxManifestReader> ManifestReader;
	if (PathIsActuallyPackage)
	{
		TComPtr<IAppxPackageReader> PackageReader;
		if (FAILED(AppxFactory->CreatePackageReader(ReaderStream, &PackageReader)))
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
		if (FAILED(AppxFactory->CreateManifestReader(ReaderStream, &ManifestReader)))
		{
			return false;
		}
	}

	TComPtr<IAppxManifestApplicationsEnumerator> AppEnumerator;
	if (FAILED(ManifestReader->GetApplications(&AppEnumerator)))
	{
		return false;
	}

	TComPtr<IAppxManifestApplication> ApplicationMetadata;
	if (FAILED(AppEnumerator->GetCurrent(&ApplicationMetadata)))
	{
		return false;
	}

	TComPtr<IPackageDebugSettings> PackageDebugSettings;
	if (SUCCEEDED(CoCreateInstance(CLSID_PackageDebugSettings, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&PackageDebugSettings))))
	{
		TComPtr<IAppxManifestPackageId> ManifestPackageId;
		if (SUCCEEDED(ManifestReader->GetPackageId(&ManifestPackageId)))
		{
			LPWSTR PackageFullName = nullptr;
			if (SUCCEEDED(ManifestPackageId->GetPackageFullName(&PackageFullName)))
			{
				PackageDebugSettings->EnableDebugging(PackageFullName, nullptr, nullptr);
				CoTaskMemFree(PackageFullName);
			}
		}
	}

	LPWSTR Aumid = nullptr;
	if (FAILED(ApplicationMetadata->GetAppUserModelId(&Aumid)))
	{
		return false;
	}

	bool ActivationSuccess = false;
	TComPtr<IApplicationActivationManager> ActivationManager;
	if (SUCCEEDED(CoCreateInstance(CLSID_ApplicationActivationManager, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ActivationManager))))
	{
		DWORD NativeProcessId;
		if (SUCCEEDED(ActivationManager->ActivateApplication(Aumid, *Params, AO_NONE, &NativeProcessId)))
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

#include "HideWindowsPlatformTypes.h"
