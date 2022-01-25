// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UWPTargetDevice.h"

#include "Misc/Paths.h"

#if PLATFORM_WINDOWS || PLATFORM_UWP
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "Windows/ComPointer.h"
#include "HoloLensBuildLib.h"

WindowsMixedReality::HoloLensBuildLib hlLib;

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

	return hlLib.PackageProject(*StreamPath, PathIsActuallyPackage, *Params, OutProcessId);
}

#include "Windows/HideWindowsPlatformTypes.h"
