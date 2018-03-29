// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PlatformCryptoBCryptModule.h"

DEFINE_LOG_CATEGORY(LogPlatformCryptoBCrypt);

IMPLEMENT_MODULE( FPlatformCryptoBCryptModule, PlatformCryptoBCrypt )

void FPlatformCryptoBCryptModule::StartupModule()
{
}

void FPlatformCryptoBCryptModule::ShutdownModule()
{
	for (auto& ProviderPair : ProviderCache)
	{
		const NTSTATUS CloseResult = BCryptCloseAlgorithmProvider(ProviderPair.Value, 0);
		if (!BCRYPT_SUCCESS(CloseResult))
		{
			UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FPlatformCryptoBCryptModule::ShutdownModule: failed to close provider with code %08x."), CloseResult);
		}
	}

	ProviderCache.Empty();
}

BCRYPT_ALG_HANDLE FPlatformCryptoBCryptModule::GetOrOpenProvider(const FString& ProviderName)
{
	BCRYPT_ALG_HANDLE* FoundProvider = ProviderCache.Find(ProviderName);
	if (FoundProvider != nullptr)
	{
		return *FoundProvider;
	}

	BCRYPT_ALG_HANDLE ProviderHandle = nullptr;
	const NTSTATUS OpenResult = BCryptOpenAlgorithmProvider(&ProviderHandle, *ProviderName, nullptr, 0);
	if (!BCRYPT_SUCCESS(OpenResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FPlatformCryptoBCryptModule::GetOrOpenProvider failed to open provider with code 0x%08x."), OpenResult);
		return nullptr;
	}
	
	ProviderCache.Emplace(ProviderName, ProviderHandle);
	return ProviderHandle;
}
