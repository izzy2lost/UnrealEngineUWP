// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "UWPHttp.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

bool bUseCurl = true;

void FUWPHttp::Init()
{
	if (GConfig)
	{
		bool bUseCurlConfigValue = false;
		if (GConfig->GetBool(TEXT("Networking"), TEXT("UseLibCurl"), bUseCurlConfigValue, GEngineIni))
		{
			bUseCurl = bUseCurlConfigValue;
		}
	}

	// allow override on command line
	FString HttpMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("HTTP="), HttpMode) &&
		(HttpMode.Equals(TEXT("WinInet"), ESearchCase::IgnoreCase)))
	{
		bUseCurl = false;
	}

	FCurlHttpManager::InitCurl();
}

void FUWPHttp::Shutdown()
{
	FCurlHttpManager::ShutdownCurl();
}

FHttpManager * FUWPHttp::CreatePlatformHttpManager()
{
	return new FCurlHttpManager();
}

IHttpRequest* FUWPHttp::ConstructRequest()
{
	return new FCurlHttpRequest(FCurlHttpManager::GMultiHandle);
}