// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "UWPHttp.h"
#include "Curl/CurlHttp.h"
#include "Curl/CurlHttpManager.h"

bool bUseCurl = true;

void FUWPHttp::Init()
{
}

void FUWPHttp::Shutdown()
{
}

FHttpManager * FUWPHttp::CreatePlatformHttpManager()
{
	return nullptr;
}

IHttpRequest* FUWPHttp::ConstructRequest()
{
	return FGenericPlatformHttp::ConstructRequest();
}