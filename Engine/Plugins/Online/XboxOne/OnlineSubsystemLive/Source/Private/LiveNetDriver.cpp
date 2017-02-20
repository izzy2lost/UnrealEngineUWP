// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "Engine.h"
#include "LiveNetDriver.h"

ULiveNetDriver::ULiveNetDriver(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

int ULiveNetDriver::GetClientPort()
{
	// Xbox One requires clients to use the port that was specified in the networking manifest.
	int Port = 0;
	GConfig->GetInt(TEXT("URL"), TEXT("Port"), Port, GEngineIni);
	return Port;
}
