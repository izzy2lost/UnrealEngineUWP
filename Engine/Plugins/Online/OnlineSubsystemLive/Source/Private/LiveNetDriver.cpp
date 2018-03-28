// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "LiveNetDriver.h"
#include "Misc/ConfigCacheIni.h"
// @ATG_CHANGE : BEGIN Adding XIM
#include "XimNetConnection.h"
// @ATG_CHANGE : END

ULiveNetDriver::ULiveNetDriver(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// @ATG_CHANGE : BEGIN
#if USE_XIM
	// TODO: investigate why XIM is timing out
	bNoTimeouts = true;
#endif
	// @ATG_CHANGE : END
}

int ULiveNetDriver::GetClientPort()
{
	// Xbox One requires clients to use the port that was specified in the networking manifest.
	int Port = 0;
	GConfig->GetInt(TEXT("URL"), TEXT("Port"), Port, GEngineIni);
	return Port;
}

// @ATG_CHANGE : BEGIN Adding XIM
bool ULiveNetDriver::InitConnectionClass()
{
#if USE_XIM
	// Only the XimNetConnection is supported when using XIM.
	NetConnectionClass = UXimNetConnection::StaticClass();
	return NetConnectionClass != nullptr;
#else
	return Super::InitConnectionClass();
#endif
}
// @ATG_CHANGE : END
