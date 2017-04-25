// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "RHI.h"
#include "ModuleManager.h"

FDynamicRHI* PlatformCreateDynamicRHI()
{
	FDynamicRHI* DynamicRHI = NULL;
	IDynamicRHIModule* DynamicRHIModule = NULL;

	const bool bForceD3D12 = FParse::Param(FCommandLine::Get(), TEXT("d3d12")) || FParse::Param(FCommandLine::Get(), TEXT("dx12"));
	if (bForceD3D12)
	{
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("D3D12RHI"));
	}
	else
	{
		// Load the dynamic RHI module.
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("D3D11RHI"));
	}

	// Create the dynamic RHI.
	if (!DynamicRHIModule->IsSupported())
	{
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("UWP", "FailedToCreateUWP_RHI", "UWPRHI failure?"));
		FPlatformMisc::RequestExit(1);
		DynamicRHIModule = NULL;
	}
	else
	{
		DynamicRHI = DynamicRHIModule->CreateRHI();
	}

	return DynamicRHI;
}
